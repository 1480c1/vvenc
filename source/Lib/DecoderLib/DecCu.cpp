/* -----------------------------------------------------------------------------
Software Copyright License for the Fraunhofer Software Library VVenc

(c) Copyright (2019-2020) Fraunhofer-Gesellschaft zur Förderung der angewandten Forschung e.V. 

1.    INTRODUCTION

The Fraunhofer Software Library VVenc (“Fraunhofer Versatile Video Encoding Library”) is software that implements (parts of) the Versatile Video Coding Standard - ITU-T H.266 | MPEG-I - Part 3 (ISO/IEC 23090-3) and related technology. 
The standard contains Fraunhofer patents as well as third-party patents. Patent licenses from third party standard patent right holders may be required for using the Fraunhofer Versatile Video Encoding Library. It is in your responsibility to obtain those if necessary. 

The Fraunhofer Versatile Video Encoding Library which mean any source code provided by Fraunhofer are made available under this software copyright license. 
It is based on the official ITU/ISO/IEC VVC Test Model (VTM) reference software whose copyright holders are indicated in the copyright notices of its source files. The VVC Test Model (VTM) reference software is licensed under the 3-Clause BSD License and therefore not subject of this software copyright license.

2.    COPYRIGHT LICENSE

Internal use of the Fraunhofer Versatile Video Encoding Library, in source and binary forms, with or without modification, is permitted without payment of copyright license fees for non-commercial purposes of evaluation, testing and academic research. 

No right or license, express or implied, is granted to any part of the Fraunhofer Versatile Video Encoding Library except and solely to the extent as expressly set forth herein. Any commercial use or exploitation of the Fraunhofer Versatile Video Encoding Library and/or any modifications thereto under this license are prohibited.

For any other use of the Fraunhofer Versatile Video Encoding Library than permitted by this software copyright license You need another license from Fraunhofer. In such case please contact Fraunhofer under the CONTACT INFORMATION below.

3.    LIMITED PATENT LICENSE

As mentioned under 1. Fraunhofer patents are implemented by the Fraunhofer Versatile Video Encoding Library. If You use the Fraunhofer Versatile Video Encoding Library in Germany, the use of those Fraunhofer patents for purposes of testing, evaluating and research and development is permitted within the statutory limitations of German patent law. However, if You use the Fraunhofer Versatile Video Encoding Library in a country where the use for research and development purposes is not permitted without a license, you must obtain an appropriate license from Fraunhofer. It is Your responsibility to check the legal requirements for any use of applicable patents.    

Fraunhofer provides no warranty of patent non-infringement with respect to the Fraunhofer Versatile Video Encoding Library.


4.    DISCLAIMER

The Fraunhofer Versatile Video Encoding Library is provided by Fraunhofer "AS IS" and WITHOUT ANY EXPRESS OR IMPLIED WARRANTIES, including but not limited to the implied warranties fitness for a particular purpose. IN NO EVENT SHALL FRAUNHOFER BE LIABLE for any direct, indirect, incidental, special, exemplary, or consequential damages, including but not limited to procurement of substitute goods or services; loss of use, data, or profits, or business interruption, however caused and on any theory of liability, whether in contract, strict liability, or tort (including negligence), arising in any way out of the use of the Fraunhofer Versatile Video Encoding Library, even if advised of the possibility of such damage.

5.    CONTACT INFORMATION

Fraunhofer Heinrich Hertz Institute
Attention: Video Coding & Analytics Department
Einsteinufer 37
10587 Berlin, Germany
www.hhi.fraunhofer.de/vvc
vvc@hhi.fraunhofer.de
----------------------------------------------------------------------------- */


/** \file     DecCu.cpp
    \brief    CU decoder class
*/

#include "DecCu.h"

#include "CommonLib/InterPrediction.h"
#include "CommonLib/IntraPrediction.h"
#include "CommonLib/Picture.h"
#include "CommonLib/UnitTools.h"
#include "CommonLib/LoopFilter.h"
#include "CommonLib/Reshape.h"
#include "CommonLib/dtrace_buffer.h"

//! \ingroup DecoderLib
//! \{

namespace vvenc {

// ====================================================================================================================
// Constructor / destructor / create / destroy
// ====================================================================================================================

DecCu::DecCu()
{
}

DecCu::~DecCu()
{
  m_TmpBuffer.destroy();
  m_PredBuffer.destroy();
}

void DecCu::init( TrQuant* pcTrQuant, IntraPrediction* pcIntra, InterPrediction* pcInter, ChromaFormat chrFormat)
{
  m_pcTrQuant       = pcTrQuant;
  m_pcIntraPred     = pcIntra;
  m_pcInterPred     = pcInter;
  m_TmpBuffer.destroy();
  m_TmpBuffer.create( chrFormat, Area( 0,0, MAX_TB_SIZEY, MAX_TB_SIZEY ));
  m_PredBuffer.destroy();
  m_PredBuffer.create( chrFormat, Area( 0,0, MAX_CU_SIZE, MAX_CU_SIZE ));
}

// ====================================================================================================================
// Public member functions
// ====================================================================================================================

void DecCu::decompressCtu( CodingStructure& cs, const UnitArea& ctuArea )
{
  const int maxNumChannelType = cs.pcv->chrFormat != CHROMA_400 && CS::isDualITree( cs ) ? 2 : 1;
  m_pcIntraPred->reset();

  for( int ch = 0; ch < maxNumChannelType; ch++ )
  {
    const ChannelType chType = ChannelType( ch );

    for( auto &currCU : cs.traverseCUs( CS::getArea( cs, ctuArea, chType, TREE_D ), chType ) )
    {

      if (currCU.predMode != MODE_INTRA && currCU.predMode != MODE_PLT && currCU.Y().valid())
      {
        xDeriveCUMV(currCU);
      }

      switch( currCU.predMode )
      {
      case MODE_INTER:
      case MODE_IBC:
        xReconInter( currCU );
        break;
      case MODE_PLT:
      case MODE_INTRA:
        xReconIntraQT( currCU );
        break;
      default:
        THROW( "Invalid prediction mode" );
        break;
      }

      LoopFilter::calcFilterStrengths(currCU);

      DTRACE_BLOCK_REC( cs.picture->getRecoBuf( currCU ), currCU, currCU.predMode );
    }
  }
}

// ====================================================================================================================
// Protected member functions
// ====================================================================================================================

void DecCu::xIntraRecBlk( TransformUnit& tu, const ComponentID compID )
{
  if( !tu.blocks[ compID ].valid() )
  {
    return;
  }

        CodingStructure &cs = *tu.cs;
  const CompArea& area      = tu.blocks[compID];

  const ChannelType chType  = toChannelType( compID );

        PelBuf piPred       = m_PredBuffer.getCompactBuf( area );

  const PredictionUnit &pu  = *tu.cs->getPU( area.pos(), chType );
  const uint32_t uiChFinalMode  = PU::getFinalIntraMode( pu, chType );
  PelBuf pReco              = cs.getRecoBuf(area);

  //===== init availability pattern =====
  bool predRegDiffFromTB = CU::isPredRegDiffFromTB(*tu.cu, compID);
  bool firstTBInPredReg = CU::isFirstTBInPredReg(*tu.cu, compID, area);
  CompArea areaPredReg(COMP_Y, tu.chromaFormat, area);
  {
    m_pcIntraPred->initIntraPatternChType(*tu.cu, area);
  }

  //===== get prediction signal =====
  if( compID != COMP_Y && PU::isLMCMode( uiChFinalMode ) )
  {
    const PredictionUnit& pu = *tu.cu->pu;
    m_pcIntraPred->loadLMLumaRecPels( pu, area );
    m_pcIntraPred->predIntraChromaLM( compID, piPred, pu, area, uiChFinalMode );
  }
  else if( PU::isMIP( pu, chType ) )
  {
    m_pcIntraPred->initIntraMip( pu );
    m_pcIntraPred->predIntraMip( piPred, pu );
  }
  else
  {
    if (predRegDiffFromTB)
    {
      if (firstTBInPredReg)
      {
        PelBuf piPredReg = cs.getPredBuf(areaPredReg);
        m_pcIntraPred->predIntraAng(compID, piPredReg, pu);
      }
    }
    else
    {
      m_pcIntraPred->predIntraAng(compID, piPred, pu);
    }
  }
  //===== inverse transform =====
  const Slice& slice       = *cs.slice;
  ReshapeData& reshapeData = cs.picture->reshapeData;
  bool lmcsflag = slice.lmcsEnabled && (slice.isIntra() || (!slice.isIntra() && reshapeData.getCTUFlag()));
  if (lmcsflag && slice.picHeader->lmcsChromaResidualScale && (compID != COMP_Y) && (tu.cbf[COMP_Cb] || tu.cbf[COMP_Cr]))
  {
    const Area area = tu.Y().valid() ? tu.Y() : Area(recalcPosition(tu.chromaFormat, tu.chType, CH_L, tu.blocks[tu.chType].pos()), recalcSize(tu.chromaFormat, tu.chType, CH_L, tu.blocks[tu.chType].size()));
    const CompArea& areaY = CompArea(COMP_Y, tu.chromaFormat, area);
    tu.chromaAdj = reshapeData.calculateChromaAdjVpduNei(tu, areaY, TREE_D);
  }
  //===== inverse transform =====
  PelBuf piResi = cs.getResiBuf( area );

  const QpParam cQP( tu, compID );

  if( tu.jointCbCr && isChroma(compID) )
  {
    if( compID == COMP_Cb )
    {
      PelBuf resiCr = cs.getResiBuf( tu.blocks[ COMP_Cr ] );
      if( tu.jointCbCr >> 1 )
      {
        m_pcTrQuant->invTransformNxN( tu, COMP_Cb, piResi, cQP );
      }
      else
      {
        const QpParam qpCr( tu, COMP_Cr );
        m_pcTrQuant->invTransformNxN( tu, COMP_Cr, resiCr, qpCr );
      }
      m_pcTrQuant->invTransformICT( tu, piResi, resiCr );
    }
  }
  else
  if( TU::getCbf( tu, compID ) )
  {
    m_pcTrQuant->invTransformNxN( tu, compID, piResi, cQP );
  }
  else
  {
    piResi.fill( 0 );
  }

  //===== reconstruction =====
  lmcsflag &= (tu.blocks[compID].width*tu.blocks[compID].height > 4) && slice.picHeader->lmcsChromaResidualScale;
  if (lmcsflag && (TU::getCbf(tu, compID) || tu.jointCbCr) && isChroma(compID) )
  {
    piResi.scaleSignal(tu.chromaAdj, 0, tu.cu->cs->slice->clpRngs[compID]);
  }

  if( !tu.cu->ispMode || !isLuma( compID ) )
  {
    cs.setDecomp( area );
  }
  else if( tu.cu->ispMode && isLuma( compID ) && CU::isISPFirst( *tu.cu, tu.blocks[compID], compID ) )
  {
    cs.setDecomp( tu.cu->blocks[compID] );
  }

  piPred.reconstruct( piPred, piResi, tu.cu->cs->slice->clpRngs[ compID ] );
  pReco.copyFrom( piPred );

  if( cs.pcv->isEncoder )
  {
    cs.picture->getRecoBuf( area ).copyFrom( pReco );
  }
}

void DecCu::xReconIntraQT( CodingUnit &cu )
{
  const uint32_t numChType = getNumberValidChannels( cu.chromaFormat );

  for( uint32_t chType = CH_L; chType < numChType; chType++ )
  {
    if( cu.blocks[chType].valid() )
    {
      xIntraRecQT( cu, ChannelType( chType ) );
    }
  }
}

/** Function for deriving reconstructed PU/CU chroma samples with QTree structure
* \param pcRecoYuv pointer to reconstructed sample arrays
* \param pcPredYuv pointer to prediction sample arrays
* \param pcResiYuv pointer to residue sample arrays
* \param chType    texture channel type (luma/chroma)
* \param rTu       reference to transform data
*
\ This function derives reconstructed PU/CU chroma samples with QTree recursive structure
*/

void DecCu::xIntraRecQT(CodingUnit &cu, const ChannelType chType)
{
  for( auto &currTU : CU::traverseTUs( cu ) )
  {
    if( isLuma( chType ) )
    {
      xIntraRecBlk( currTU, COMP_Y );
    }
    else
    {
      const uint32_t numValidComp = getNumberValidComponents( cu.chromaFormat );

      for( uint32_t compID = COMP_Cb; compID < numValidComp; compID++ )
      {
        xIntraRecBlk( currTU, ComponentID( compID ) );
      }
    }
  }
}


void DecCu::xReconInter(CodingUnit &cu)
{
  CodingStructure &cs = *cu.cs;
  // inter prediction

  PredictionUnit &pu = *cu.pu;
  PelUnitBuf predBuf = m_PredBuffer.getCompactBuf( pu ); 
  const ReshapeData& reshapeData = cs.picture->reshapeData;
  if (cu.geo)
  {
    m_pcInterPred->motionCompensationGeo(pu, predBuf, m_geoMrgCtx);
    PU::spanGeoMotionInfo(*cu.pu, m_geoMrgCtx, cu.pu->geoSplitDir, cu.pu->geoMergeIdx0, cu.pu->geoMergeIdx1);
  }
  else if( cu.predMode == MODE_IBC )
  {
    THROW("no IBC support");
  }
  else
  {
    cu.pu->mvRefine = true;
    m_pcInterPred->motionCompensation( pu, predBuf );
    cu.pu->mvRefine = false;

    if (!cu.affine && !cu.geo )
    {
      const MotionInfo &mi = pu.getMotionInfo();
      HPMVInfo hMi( mi, (mi.interDir == 3) ? cu.BcwIdx : BCW_DEFAULT, cu.imv == IMV_HPEL );
      cs.addMiToLut( cu.cs->motionLut.lut, hMi );
    }

    if (cu.pu->ciip)
    {
      const PredictionUnit& pu = *cu.pu;
      PelBuf ciipBuf = m_TmpBuffer.getCompactBuf( pu.Y() );

      m_pcIntraPred->initIntraPatternChType(cu, pu.Y());
      m_pcIntraPred->predIntraAng(COMP_Y, ciipBuf, pu);

      if( cs.picHeader->lmcsEnabled && reshapeData.getCTUFlag() )
      {
        predBuf.Y().rspSignal( reshapeData.getFwdLUT());
      }
      const int numCiipIntra = m_pcIntraPred->getNumIntraCiip( pu );
      predBuf.Y().weightCiip( ciipBuf, numCiipIntra);

      if (isChromaEnabled(pu.chromaFormat) && pu.chromaSize().width > 2)
      {
        PelBuf ciipBufC = m_TmpBuffer.getCompactBuf( pu.Cb() );
        m_pcIntraPred->initIntraPatternChType(cu, pu.Cb());
        m_pcIntraPred->predIntraAng(COMP_Cb, ciipBufC, pu);
        predBuf.Cb().weightCiip( ciipBufC, numCiipIntra);

        m_pcIntraPred->initIntraPatternChType(cu, pu.Cr());
        m_pcIntraPred->predIntraAng(COMP_Cr, ciipBufC, pu);
        predBuf.Cr().weightCiip( ciipBufC, numCiipIntra);
      }
    }
  }

  if (cs.slice->lmcsEnabled && reshapeData.getCTUFlag() && !cu.pu->ciip && !CU::isIBC(cu) )
  {
    predBuf.Y().rspSignal(reshapeData.getFwdLUT());
  }

  // inter recon
  xDecodeInterTexture(cu);

  if (cu.rootCbf)
  {
    cs.getResiBuf( cu ).reconstruct( predBuf, cs.getResiBuf( cu ), cs.slice->clpRngs );
    cs.getRecoBuf( cu ).copyFrom( cs.getResiBuf( cu ) );
  }
  else
  {
    cs.getRecoBuf(cu).copyClip( predBuf, cs.slice->clpRngs);
  }

  cs.setDecomp(cu);
}

void DecCu::xDecodeInterTU( TransformUnit&  currTU, const ComponentID compID )
{
  if( !currTU.blocks[compID].valid() ) return;

  const CompArea& area = currTU.blocks[compID];

  CodingStructure& cs = *currTU.cs;

  //===== inverse transform =====
  PelBuf resiBuf  = cs.getResiBuf(area);

  const QpParam cQP(currTU, compID);

  if( currTU.jointCbCr && isChroma(compID) )
  {
    if( compID == COMP_Cb )
    {
      PelBuf resiCr = cs.getResiBuf( currTU.blocks[ COMP_Cr ] );
      if( currTU.jointCbCr >> 1 )
      {
        m_pcTrQuant->invTransformNxN( currTU, COMP_Cb, resiBuf, cQP );
      }
      else
      {
        const QpParam qpCr( currTU, COMP_Cr );
        m_pcTrQuant->invTransformNxN( currTU, COMP_Cr, resiCr, qpCr );
      }
      m_pcTrQuant->invTransformICT( currTU, resiBuf, resiCr );
    }
  }
  else
  if( TU::getCbf( currTU, compID ) )
  {
    m_pcTrQuant->invTransformNxN( currTU, compID, resiBuf, cQP );
  }
  else
  {
    resiBuf.fill( 0 );
  }

  const Slice& slice = *currTU.cu->slice;
  const ReshapeData& reshapeData = cs.picture->reshapeData;
  if( slice.lmcsEnabled && reshapeData.getCTUFlag() && isChroma(compID) && (TU::getCbf(currTU, compID) || currTU.jointCbCr)
   && slice.picHeader->lmcsChromaResidualScale && currTU.blocks[compID].width * currTU.blocks[compID].height > 4)
  {
    resiBuf.scaleSignal(currTU.chromaAdj, 0, slice.clpRngs[compID]);
  }
}

void DecCu::xDecodeInterTexture(CodingUnit &cu)
{
  if( !cu.rootCbf )
  {
    return;
  }

  const uint32_t uiNumVaildComp = getNumberValidComponents(cu.chromaFormat);
  ReshapeData& reshapeData = cu.cs->picture->reshapeData;
  const bool chromaAdj = cu.slice->lmcsEnabled && reshapeData.getCTUFlag() && cu.slice->picHeader->lmcsChromaResidualScale;
  for (uint32_t ch = 0; ch < uiNumVaildComp; ch++)
  {
    const ComponentID compID = ComponentID(ch);

    for( auto& currTU : CU::traverseTUs( cu ) )
    {
      if( chromaAdj && (compID == COMP_Y) && (currTU.cbf[COMP_Cb] || currTU.cbf[COMP_Cr]))
      {
        currTU.chromaAdj = reshapeData.calculateChromaAdjVpduNei(currTU, currTU.blocks[COMP_Y], TREE_D);
      }

      xDecodeInterTU( currTU, compID );
    }
  }
}

void DecCu::xDeriveCUMV( CodingUnit &cu )
{
  PredictionUnit &pu = *cu.pu;
  {
    MergeCtx mrgCtx;

    if( pu.mergeFlag )
    {
      if (pu.mmvdMergeFlag || pu.cu->mmvdSkip)
      {
        CHECK(pu.ciip == true, "invalid MHIntra");
        if (pu.cs->sps->SbtMvp)
        {
          Size bufSize = g_miScaling.scale(pu.lumaSize());
          mrgCtx.subPuMvpMiBuf = MotionBuf(m_subPuMiBuf, bufSize);
        }
        int   fPosBaseIdx = pu.mmvdMergeIdx / MMVD_MAX_REFINE_NUM;
        PU::getInterMergeCandidates(pu, mrgCtx, 1, fPosBaseIdx + 1);
        PU::getInterMMVDMergeCandidates(pu, mrgCtx, pu.mmvdMergeIdx);
        mrgCtx.setMmvdMergeCandiInfo(pu, pu.mmvdMergeIdx);
        PU::spanMotionInfo(pu, mrgCtx);
      }
      else
      {
        if (pu.cu->geo)
        {
          PU::getGeoMergeCandidates(pu, m_geoMrgCtx);
        }
        else
        {
          if (pu.cu->affine)
          {
            AffineMergeCtx affineMergeCtx;
            if (pu.cs->sps->SbtMvp)
            {
              Size bufSize          = g_miScaling.scale(pu.lumaSize());
              mrgCtx.subPuMvpMiBuf  = MotionBuf(m_subPuMiBuf, bufSize);
              affineMergeCtx.mrgCtx = &mrgCtx;
            }
            PU::getAffineMergeCand(pu, affineMergeCtx, pu.mergeIdx);
            pu.interDir       = affineMergeCtx.interDirNeighbours[pu.mergeIdx];
            pu.cu->affineType = affineMergeCtx.affineType[pu.mergeIdx];
            pu.cu->BcwIdx     = affineMergeCtx.BcwIdx[pu.mergeIdx];
            pu.mergeType      = affineMergeCtx.mergeType[pu.mergeIdx];

            if (pu.mergeType == MRG_TYPE_SUBPU_ATMVP)
            {
              pu.refIdx[0] = affineMergeCtx.mvFieldNeighbours[(pu.mergeIdx << 1) + 0][0].refIdx;
              pu.refIdx[1] = affineMergeCtx.mvFieldNeighbours[(pu.mergeIdx << 1) + 1][0].refIdx;
            }
            else
            {
              for (int i = 0; i < 2; ++i)
              {
                if (pu.cs->slice->numRefIdx[RefPicList(i)] > 0)
                {
                  MvField *mvField = affineMergeCtx.mvFieldNeighbours[(pu.mergeIdx << 1) + i];
                  pu.mvpIdx[i]     = 0;
                  pu.mvpNum[i]     = 0;
                  pu.mvd[i]        = Mv();
                  PU::setAllAffineMvField(pu, mvField, RefPicList(i));
                }
              }
            }
          }
          else
          {
            PU::getInterMergeCandidates(pu, mrgCtx, 0, pu.mergeIdx);
            mrgCtx.setMergeInfo(pu, pu.mergeIdx);
          }

          PU::spanMotionInfo(pu, mrgCtx);
        }
      }
    } 
    else
    {
      if (pu.cu->affine)
      {
        for (uint32_t uiRefListIdx = 0; uiRefListIdx < 2; uiRefListIdx++)
        {
          RefPicList eRefList = RefPicList(uiRefListIdx);
          if (pu.cs->slice->numRefIdx[eRefList] > 0 && (pu.interDir & (1 << uiRefListIdx)))
          {
            AffineAMVPInfo affineAMVPInfo;
            PU::fillAffineMvpCand(pu, eRefList, pu.refIdx[eRefList], affineAMVPInfo);

            const unsigned mvp_idx = pu.mvpIdx[eRefList];

            pu.mvpNum[eRefList] = affineAMVPInfo.numCand;

            //    Mv mv[3];
            CHECK(pu.refIdx[eRefList] < 0, "Unexpected negative refIdx.");
            if (!cu.cs->pcv->isEncoder)
            {
              pu.mvdAffi[eRefList][0].changeAffinePrecAmvr2Internal(pu.cu->imv);
              pu.mvdAffi[eRefList][1].changeAffinePrecAmvr2Internal(pu.cu->imv);
              if (cu.affineType == AFFINEMODEL_6PARAM)
              {
                pu.mvdAffi[eRefList][2].changeAffinePrecAmvr2Internal(pu.cu->imv);
              }
            }

            Mv mvLT = affineAMVPInfo.mvCandLT[mvp_idx] + pu.mvdAffi[eRefList][0];
            Mv mvRT = affineAMVPInfo.mvCandRT[mvp_idx] + pu.mvdAffi[eRefList][1];
            mvRT += pu.mvdAffi[eRefList][0];

            Mv mvLB;
            if (cu.affineType == AFFINEMODEL_6PARAM)
            {
              mvLB = affineAMVPInfo.mvCandLB[mvp_idx] + pu.mvdAffi[eRefList][2];
              mvLB += pu.mvdAffi[eRefList][0];
            }
            PU::setAllAffineMv(pu, mvLT, mvRT, mvLB, eRefList, true);
          }
        }
      }
      else
      {
        for ( uint32_t uiRefListIdx = 0; uiRefListIdx < 2; uiRefListIdx++ )
        {
          RefPicList eRefList = RefPicList( uiRefListIdx );
          if ((pu.cs->slice->numRefIdx[eRefList] > 0 || (eRefList == REF_PIC_LIST_0 && CU::isIBC(*pu.cu))) && (pu.interDir & (1 << uiRefListIdx)))
          {
            AMVPInfo amvpInfo;
            PU::fillMvpCand(pu, eRefList, pu.refIdx[eRefList], amvpInfo);
            pu.mvpNum [eRefList] = amvpInfo.numCand;
            if (!cu.cs->pcv->isEncoder)
            {
              pu.mvd[eRefList].changeTransPrecAmvr2Internal(pu.cu->imv);
            }
            pu.mv[eRefList] = amvpInfo.mvCand[pu.mvpIdx[eRefList]] + pu.mvd[eRefList];
            pu.mv[eRefList].mvCliptoStorageBitDepth();
          }
        }
      }
      PU::spanMotionInfo( pu, mrgCtx );
    }
  }
}

} // namespace vvenc

//! \}

