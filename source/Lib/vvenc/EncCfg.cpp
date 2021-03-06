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


/** \file     EncCfg.cpp
    \brief    encoder configuration class
*/

#include "../../../include/vvenc/EncCfg.h"

#include "CommonLib/CommonDef.h"
#include "CommonLib/Slice.h"

#include <math.h>

//! \ingroup Interface
//! \{

namespace vvenc {

bool EncCfg::confirmParameter( bool bflag, const char* message )
{
  if ( ! bflag )
    return false;
  msg( ERROR, "Error: %s\n", message );
  m_confirmFailed = true;
  return true;
}

bool EncCfg::initCfgParameter()
{
#define CONFIRM_PARAMETER_OR_RETURN( _f, _m ) { if ( confirmParameter( _f, _m ) ) return true; }


  m_confirmFailed = false;


  //
  // set a lot of dependent parameters
  //
  if( m_profile == Profile::AUTO )
  {
    const int maxBitDepth= std::max(m_internalBitDepth[CH_L], m_internalBitDepth[m_internChromaFormat==ChromaFormat::CHROMA_400 ? CH_L : CH_C]);
    m_profile=Profile::NONE;

    if (m_internChromaFormat==ChromaFormat::CHROMA_400 || m_internChromaFormat==ChromaFormat::CHROMA_420)
    {
      if (maxBitDepth<=10)
      {
        m_profile=Profile::MAIN_10;
      }
    }
    else if (m_internChromaFormat==ChromaFormat::CHROMA_422 || m_internChromaFormat==ChromaFormat::CHROMA_444)
    {
      if (maxBitDepth<=10)
      {
        m_profile=Profile::MAIN_10_444;
      }
    }

    CONFIRM_PARAMETER_OR_RETURN(  m_profile == Profile::NONE, "can not determin auto profile");
  }


  if ( m_InputQueueSize <= 0 )
  {
    m_InputQueueSize = m_GOPSize;
  }
  if ( m_MCTF )
  {
    m_InputQueueSize += MCTF_ADD_QUEUE_DELAY;
  }

  m_framesToBeEncoded = ( m_framesToBeEncoded + m_temporalSubsampleRatio - 1 ) / m_temporalSubsampleRatio;

  m_MCTFNumLeadFrames  = std::min( m_MCTFNumLeadFrames,  MCTF_RANGE );
  m_MCTFNumTrailFrames = std::min( m_MCTFNumTrailFrames, MCTF_RANGE );

  if( !m_tileUniformSpacingFlag && m_numTileColumnsMinus1 > 0 )
  {
    CONFIRM_PARAMETER_OR_RETURN( m_tileColumnWidth.size() != m_numTileColumnsMinus1, "Error: The number of columns minus 1 does not match size of tile column array." );
  }
  else
  {
    m_tileColumnWidth.clear();
  }

  if( !m_tileUniformSpacingFlag && m_numTileRowsMinus1 > 0 )
  {
    CONFIRM_PARAMETER_OR_RETURN( m_tileRowHeight.size() != m_numTileRowsMinus1, "Error: The number of rows minus 1 does not match size of tile row array." );
  }
  else
  {
    m_tileRowHeight.clear();
  }

  /* rules for input, output and internal bitdepths as per help text */
  if (m_MSBExtendedBitDepth[CH_L  ] == 0)
  {
    m_MSBExtendedBitDepth[CH_L  ] = m_inputBitDepth      [CH_L  ];
  }
  if (m_MSBExtendedBitDepth[CH_C] == 0)
  {
    m_MSBExtendedBitDepth[CH_C] = m_MSBExtendedBitDepth[CH_L  ];
  }
  if (m_internalBitDepth   [CH_L  ] == 0)
  {
    m_internalBitDepth   [CH_L  ] = m_MSBExtendedBitDepth[CH_L  ];
  }
  if (m_internalBitDepth   [CH_C] == 0)
  {
    m_internalBitDepth   [CH_C] = m_internalBitDepth   [CH_L  ];
  }
  if (m_inputBitDepth      [CH_C] == 0)
  {
    m_inputBitDepth      [CH_C] = m_inputBitDepth      [CH_L  ];
  }
  if (m_outputBitDepth     [CH_L  ] == 0)
  {
    m_outputBitDepth     [CH_L  ] = m_internalBitDepth   [CH_L  ];
  }
  if (m_outputBitDepth     [CH_C] == 0)
  {
    m_outputBitDepth     [CH_C] = m_outputBitDepth     [CH_L  ];
  }

  CONFIRM_PARAMETER_OR_RETURN( m_fastInterSearchMode<0 || m_fastInterSearchMode>FASTINTERSEARCH_MODE3, "Error: FastInterSearchMode parameter out of range" );

  CONFIRM_PARAMETER_OR_RETURN( m_motionEstimationSearchMethod < 0 || m_motionEstimationSearchMethod >= MESEARCH_NUMBER_OF_METHODS, "Error: FastSearch parameter out of range" );

  switch (m_conformanceWindowMode)
  {
  case 0:
    {
      // no conformance or padding
      m_confWinLeft = m_confWinRight = m_confWinTop = m_confWinBottom = 0;
      m_aiPad[1] = m_aiPad[0] = 0;
      break;
    }
  case 1:
    {
      // automatic padding to minimum CU size
      const int minCuSize = 1 << ( MIN_CU_LOG2 + 1 );
      if (m_SourceWidth % minCuSize)
      {
        m_aiPad[0] = m_confWinRight  = ((m_SourceWidth / minCuSize) + 1) * minCuSize - m_SourceWidth;
        m_SourceWidth  += m_confWinRight;
      }
      if (m_SourceHeight % minCuSize)
      {
        m_aiPad[1] = m_confWinBottom = ((m_SourceHeight / minCuSize) + 1) * minCuSize - m_SourceHeight;
        m_SourceHeight += m_confWinBottom;
      }
      CONFIRM_PARAMETER_OR_RETURN( m_aiPad[0] % SPS::getWinUnitX(m_internChromaFormat) != 0, "Error: picture width is not an integer multiple of the specified chroma subsampling" );
      CONFIRM_PARAMETER_OR_RETURN( m_aiPad[1] % SPS::getWinUnitY(m_internChromaFormat) != 0, "Error: picture height is not an integer multiple of the specified chroma subsampling" );
      break;
    }
  case 2:
    {
      //padding
      m_SourceWidth  += m_aiPad[0];
      m_SourceHeight += m_aiPad[1];
      m_confWinRight  = m_aiPad[0];
      m_confWinBottom = m_aiPad[1];
      break;
    }
  case 3:
    {
      // conformance
      if ((m_confWinLeft == 0) && (m_confWinRight == 0) && (m_confWinTop == 0) && (m_confWinBottom == 0))
      {
        msg( ERROR, "Warning: Conformance window enabled, but all conformance window parameters set to zero\n");
      }
      if ((m_aiPad[1] != 0) || (m_aiPad[0]!=0))
      {
        msg( ERROR, "Warning: Conformance window enabled, padding parameters will be ignored\n");
      }
      m_aiPad[1] = m_aiPad[0] = 0;
      break;
    }
  }

  m_topLeftBrickIdx.clear();
  m_bottomRightBrickIdx.clear();
  m_sliceId.clear();

  bool singleTileInPicFlag = (m_numTileRowsMinus1 == 0 && m_numTileColumnsMinus1 == 0);

  if (!singleTileInPicFlag)
  {
    //if (!m_singleBrickPerSliceFlag && m_rectSliceFlag)
    if (/*m_sliceMode != 0 && m_sliceMode != 4 && */m_rectSliceFlag)
    {
      int numSlicesInPic = m_numSlicesInPicMinus1 + 1;

      CONFIRM_PARAMETER_OR_RETURN( m_rectSliceBoundary.size() > numSlicesInPic * 2, "Error: The number of slice indices (RectSlicesBoundaryInPic) is greater than the NumSlicesInPicMinus1." );
      CONFIRM_PARAMETER_OR_RETURN( m_rectSliceBoundary.size() < numSlicesInPic * 2, "Error: The number of slice indices (RectSlicesBoundaryInPic) is less than the NumSlicesInPicMinus1." );

      m_topLeftBrickIdx.resize(numSlicesInPic);
      m_bottomRightBrickIdx.resize(numSlicesInPic);
      for (uint32_t i = 0; i < numSlicesInPic; ++i)
      {
        m_topLeftBrickIdx[i] = m_rectSliceBoundary[i * 2];
        m_bottomRightBrickIdx[i] = m_rectSliceBoundary[i * 2 + 1];
      }
      //Validating the correctness of rectangular slice structure
      int **brickToSlice = (int **)malloc(sizeof(int *) * (m_numTileRowsMinus1 + 1));
      for (int i = 0; i <= m_numTileRowsMinus1; i++)
      {
        brickToSlice[i] = (int *)malloc(sizeof(int) * (m_numTileColumnsMinus1 + 1));
        memset(brickToSlice[i], -1, sizeof(int) * ((m_numTileColumnsMinus1 + 1)));
      }

      //Check overlap case
      for (int sliceIdx = 0; sliceIdx < numSlicesInPic; sliceIdx++)
      {
        int sliceStartRow = m_topLeftBrickIdx[sliceIdx] / (m_numTileColumnsMinus1 + 1);
        int sliceEndRow   = m_bottomRightBrickIdx[sliceIdx] / (m_numTileColumnsMinus1 + 1);
        int sliceStartCol = m_topLeftBrickIdx[sliceIdx] % (m_numTileColumnsMinus1 + 1);
        int sliceEndCol   = m_bottomRightBrickIdx[sliceIdx] % (m_numTileColumnsMinus1 + 1);
        for (int i = 0; i <= m_numTileRowsMinus1; i++)
        {
          for (int j = 0; j <= m_numTileColumnsMinus1; j++)
          {
            if (i >= sliceStartRow && i <= sliceEndRow && j >= sliceStartCol && j <= sliceEndCol)
            {
              CONFIRM_PARAMETER_OR_RETURN( brickToSlice[i][j] != -1, "Error: Values given in RectSlicesBoundaryInPic have conflict! Rectangular slice shall not have overlapped tile(s)" );
              brickToSlice[i][j] = sliceIdx;
            }
          }
        }
      }
      //Check gap case
      for (int i = 0; i <= m_numTileRowsMinus1; i++)
      {
        for (int j = 0; j <= m_numTileColumnsMinus1; j++)
        {
          CONFIRM_PARAMETER_OR_RETURN( brickToSlice[i][j] == -1, "Error: Values given in RectSlicesBoundaryInPic have conflict! Rectangular slice shall not have gap" );
        }
      }

      for (int i = 0; i <= m_numTileRowsMinus1; i++)
      {
        free(brickToSlice[i]);
        brickToSlice[i] = 0;
      }
      free(brickToSlice);
      brickToSlice = 0;
    }      // (!m_singleBrickPerSliceFlag && m_rectSliceFlag)
  }        // !singleTileInPicFlag

  if (m_rectSliceFlag && m_signalledSliceIdFlag)
  {
    int numSlicesInPic = m_numSlicesInPicMinus1 + 1;

    CONFIRM_PARAMETER_OR_RETURN( m_signalledSliceId.size() > numSlicesInPic, "Error: The number of Slice Ids are greater than the m_signalledTileGroupIdLengthMinus1." );
    CONFIRM_PARAMETER_OR_RETURN( m_signalledSliceId.size() < numSlicesInPic, "Error: The number of Slice Ids are less than the m_signalledTileGroupIdLengthMinus1." );
    m_sliceId.resize(numSlicesInPic);
    for (uint32_t i = 0; i < m_signalledSliceId.size(); ++i)
    {
      m_sliceId[i] = m_signalledSliceId[i];
    }
  }
  else if (m_rectSliceFlag)
  {
    int numSlicesInPic = m_numSlicesInPicMinus1 + 1;
    m_sliceId.resize(numSlicesInPic);
    for (uint32_t i = 0; i < numSlicesInPic; ++i)
    {
      m_sliceId[i] = i;
    }
  }

  for(uint32_t ch=0; ch < MAX_NUM_CH; ch++ )
  {
    if (m_saoOffsetBitShift[ch]<0)
    {
      if (m_internalBitDepth[ch]>10)
      {
        m_log2SaoOffsetScale[ch]=uint32_t(Clip3<int>(0, m_internalBitDepth[ch]-10, int(m_internalBitDepth[ch]-10 + 0.165*m_QP - 3.22 + 0.5) ) );
      }
      else
      {
        m_log2SaoOffsetScale[ch]=0;
      }
    }
    else
    {
      m_log2SaoOffsetScale[ch]=uint32_t(m_saoOffsetBitShift[ch]);
    }
  }

  CONFIRM_PARAMETER_OR_RETURN(m_qpInValsCb.size() != m_qpOutValsCb.size(), "Chroma QP table for Cb is incomplete.");
  CONFIRM_PARAMETER_OR_RETURN(m_qpInValsCr.size() != m_qpOutValsCr.size(), "Chroma QP table for Cr is incomplete.");
  CONFIRM_PARAMETER_OR_RETURN(m_qpInValsCbCr.size() != m_qpOutValsCbCr.size(), "Chroma QP table for CbCr is incomplete.");
  if (m_useIdentityTableForNon420Chroma && m_internChromaFormat != CHROMA_420)
  {
    m_chromaQpMappingTableParams.m_sameCQPTableForAllChromaFlag = true;
    m_qpInValsCb = { 0 };
    m_qpInValsCr = { 0 };
    m_qpInValsCbCr = { 0 };
    m_qpOutValsCb = { 0 };
    m_qpOutValsCr = { 0 };
    m_qpOutValsCbCr = { 0 };
  }
  int qpBdOffsetC = 6 * (m_internalBitDepth[CH_C] - 8);

  m_chromaQpMappingTableParams.m_numQpTables = m_chromaQpMappingTableParams.m_sameCQPTableForAllChromaFlag? 1 : (m_JointCbCrMode ? 3 : 2);
  m_chromaQpMappingTableParams.m_deltaQpInValMinus1[0].resize(m_qpInValsCb.size());
  m_chromaQpMappingTableParams.m_deltaQpOutVal[0].resize(m_qpOutValsCb.size());
  m_chromaQpMappingTableParams.m_numPtsInCQPTableMinus1[0] = (m_qpOutValsCb.size() > 1) ? (int)m_qpOutValsCb.size() - 2 : 0;
  m_chromaQpMappingTableParams.m_qpTableStartMinus26[0] = (m_qpOutValsCb.size() > 1) ? -26 + m_qpInValsCb[0] : 0;
  CONFIRM_PARAMETER_OR_RETURN(m_chromaQpMappingTableParams.m_qpTableStartMinus26[0] < -26 - qpBdOffsetC || m_chromaQpMappingTableParams.m_qpTableStartMinus26[0] > 36, "qpTableStartMinus26[0] is out of valid range of -26 -qpBdOffsetC to 36, inclusive.")
  CONFIRM_PARAMETER_OR_RETURN(m_qpInValsCb[0] != m_qpOutValsCb[0], "First qpInValCb value should be equal to first qpOutValCb value");
  for (int i = 0; i < m_qpInValsCb.size() - 1; i++)
  {
    CONFIRM_PARAMETER_OR_RETURN(m_qpInValsCb[i] < -qpBdOffsetC || m_qpInValsCb[i] > MAX_QP, "Some entries cfg_qpInValCb are out of valid range of -qpBdOffsetC to 63, inclusive.");
    CONFIRM_PARAMETER_OR_RETURN(m_qpOutValsCb[i] < -qpBdOffsetC || m_qpOutValsCb[i] > MAX_QP, "Some entries cfg_qpOutValCb are out of valid range of -qpBdOffsetC to 63, inclusive.");
    m_chromaQpMappingTableParams.m_deltaQpInValMinus1[0][i] = m_qpInValsCb[i + 1] - m_qpInValsCb[i] - 1;
    m_chromaQpMappingTableParams.m_deltaQpOutVal[0][i] = m_qpOutValsCb[i + 1] - m_qpOutValsCb[i];
  }
  if (!m_chromaQpMappingTableParams.m_sameCQPTableForAllChromaFlag)
  {
    m_chromaQpMappingTableParams.m_deltaQpInValMinus1[1].resize(m_qpInValsCr.size());
    m_chromaQpMappingTableParams.m_deltaQpOutVal[1].resize(m_qpOutValsCr.size());
    m_chromaQpMappingTableParams.m_numPtsInCQPTableMinus1[1] = (m_qpOutValsCr.size() > 1) ? (int)m_qpOutValsCr.size() - 2 : 0;
    m_chromaQpMappingTableParams.m_qpTableStartMinus26[1] = (m_qpOutValsCr.size() > 1) ? -26 + m_qpInValsCr[0] : 0;
    CONFIRM_PARAMETER_OR_RETURN(m_chromaQpMappingTableParams.m_qpTableStartMinus26[1] < -26 - qpBdOffsetC || m_chromaQpMappingTableParams.m_qpTableStartMinus26[1] > 36, "qpTableStartMinus26[1] is out of valid range of -26 -qpBdOffsetC to 36, inclusive.")
    CONFIRM_PARAMETER_OR_RETURN(m_qpInValsCr[0] != m_qpOutValsCr[0], "First qpInValCr value should be equal to first qpOutValCr value");
    for (int i = 0; i < m_qpInValsCr.size() - 1; i++)
    {
      CONFIRM_PARAMETER_OR_RETURN(m_qpInValsCr[i] < -qpBdOffsetC || m_qpInValsCr[i] > MAX_QP, "Some entries cfg_qpInValCr are out of valid range of -qpBdOffsetC to 63, inclusive.");
      CONFIRM_PARAMETER_OR_RETURN(m_qpOutValsCr[i] < -qpBdOffsetC || m_qpOutValsCr[i] > MAX_QP, "Some entries cfg_qpOutValCr are out of valid range of -qpBdOffsetC to 63, inclusive.");
      m_chromaQpMappingTableParams.m_deltaQpInValMinus1[1][i] = m_qpInValsCr[i + 1] - m_qpInValsCr[i] - 1;
      m_chromaQpMappingTableParams.m_deltaQpOutVal[1][i] = m_qpOutValsCr[i + 1] - m_qpOutValsCr[i];
    }
    m_chromaQpMappingTableParams.m_deltaQpInValMinus1[2].resize(m_qpInValsCbCr.size());
    m_chromaQpMappingTableParams.m_deltaQpOutVal[2].resize(m_qpOutValsCbCr.size());
    m_chromaQpMappingTableParams.m_numPtsInCQPTableMinus1[2] = (m_qpOutValsCbCr.size() > 1) ? (int)m_qpOutValsCbCr.size() - 2 : 0;
    m_chromaQpMappingTableParams.m_qpTableStartMinus26[2] = (m_qpOutValsCbCr.size() > 1) ? -26 + m_qpInValsCbCr[0] : 0;
    CONFIRM_PARAMETER_OR_RETURN(m_chromaQpMappingTableParams.m_qpTableStartMinus26[2] < -26 - qpBdOffsetC || m_chromaQpMappingTableParams.m_qpTableStartMinus26[2] > 36, "qpTableStartMinus26[2] is out of valid range of -26 -qpBdOffsetC to 36, inclusive.")
    CONFIRM_PARAMETER_OR_RETURN(m_qpInValsCbCr[0] != m_qpInValsCbCr[0], "First qpInValCbCr value should be equal to first qpOutValCbCr value");
    for (int i = 0; i < m_qpInValsCbCr.size() - 1; i++)
    {
      CONFIRM_PARAMETER_OR_RETURN(m_qpInValsCbCr[i] < -qpBdOffsetC || m_qpInValsCbCr[i] > MAX_QP, "Some entries cfg_qpInValCbCr are out of valid range of -qpBdOffsetC to 63, inclusive.");
      CONFIRM_PARAMETER_OR_RETURN(m_qpOutValsCbCr[i] < -qpBdOffsetC || m_qpOutValsCbCr[i] > MAX_QP, "Some entries cfg_qpOutValCbCr are out of valid range of -qpBdOffsetC to 63, inclusive.");
      m_chromaQpMappingTableParams.m_deltaQpInValMinus1[2][i] = m_qpInValsCbCr[i + 1] - m_qpInValsCbCr[i] - 1;
      m_chromaQpMappingTableParams.m_deltaQpOutVal[2][i] = m_qpInValsCbCr[i + 1] - m_qpInValsCbCr[i];
    }
  }

  const int minCuSize = 1 << MIN_CU_LOG2;
  m_MaxCodingDepth = 0;
  while( ( m_CTUSize >> m_MaxCodingDepth ) > minCuSize )
  {
    m_MaxCodingDepth++;
  }
  m_log2DiffMaxMinCodingBlockSize = m_MaxCodingDepth;

  m_reshapeCW.binCW.resize(3);
  m_reshapeCW.rspFps     = m_FrameRate;
  m_reshapeCW.rspPicSize = m_SourceWidth*m_SourceHeight;
  m_reshapeCW.rspFpsToIp = std::max(16, 16 * (int)(round((double)m_FrameRate /16.0)));
  m_reshapeCW.rspBaseQP  = m_QP;
  m_reshapeCW.updateCtrl = m_updateCtrl;
  m_reshapeCW.adpOption  = m_adpOption;
  m_reshapeCW.initialCW  = m_initialCW;


  //
  // do some check and set of parameters next
  //


  msg( NOTICE, "\n" );
  if ( m_decodedPictureHashSEIType == HASHTYPE_NONE )
  {
    msg( DETAILS, "******************************************************************\n");
    msg( DETAILS, "** WARNING: --SEIDecodedPictureHash is now disabled by default. **\n");
    msg( DETAILS, "**          Automatic verification of decoded pictures by a     **\n");
    msg( DETAILS, "**          decoder requires this option to be enabled.         **\n");
    msg( DETAILS, "******************************************************************\n");
  }
  if( m_profile == Profile::NONE )
  {
    msg( DETAILS, "***************************************************************************\n");
    msg( DETAILS, "** WARNING: For conforming bitstreams a valid Profile value must be set! **\n");
    msg( DETAILS, "***************************************************************************\n");
  }
  if( m_level == Level::NONE )
  {
    msg( DETAILS, "***************************************************************************\n");
    msg( DETAILS, "** WARNING: For conforming bitstreams a valid Level value must be set!   **\n");
    msg( DETAILS, "***************************************************************************\n");
  }

  if( m_DepQuantEnabled )
  {
    confirmParameter( !m_RDOQ || !m_useRDOQTS, "RDOQ and RDOQTS must be greater 0 if dependent quantization is enabled" );
    confirmParameter( m_SignDataHidingEnabled, "SignHideFlag must be equal to 0 if dependent quantization is enabled" );
  }

  confirmParameter( (m_MSBExtendedBitDepth[CH_L  ] < m_inputBitDepth[CH_L  ]), "MSB-extended bit depth for luma channel (--MSBExtendedBitDepth) must be greater than or equal to input bit depth for luma channel (--InputBitDepth)" );
  confirmParameter( (m_MSBExtendedBitDepth[CH_C] < m_inputBitDepth[CH_C]), "MSB-extended bit depth for chroma channel (--MSBExtendedBitDepthC) must be greater than or equal to input bit depth for chroma channel (--InputBitDepthC)" );

  const uint32_t maxBitDepth=(m_internChromaFormat==CHROMA_400) ? m_internalBitDepth[CH_L] : std::max(m_internalBitDepth[CH_L], m_internalBitDepth[CH_C]);
  confirmParameter(m_bitDepthConstraintValue<maxBitDepth, "The internalBitDepth must not be greater than the bitDepthConstraint value");

  confirmParameter(m_bitDepthConstraintValue!=10, "BitDepthConstraint must be 8 for MAIN profile and 10 for MAIN10 profile.");
  confirmParameter(m_intraOnlyConstraintFlag==true, "IntraOnlyConstraintFlag must be false for non main_RExt profiles.");

  // check range of parameters
  confirmParameter( m_inputBitDepth[CH_L  ] < 8,                                   "InputBitDepth must be at least 8" );
  confirmParameter( m_inputBitDepth[CH_C] < 8,                                   "InputBitDepthC must be at least 8" );

#if !RExt__HIGH_BIT_DEPTH_SUPPORT
  for (uint32_t channelType = 0; channelType < MAX_NUM_CH; channelType++)
  {
    confirmParameter((m_internalBitDepth[channelType] > 12) , "Model is not configured to support high enough internal accuracies - enable RExt__HIGH_BIT_DEPTH_SUPPORT to use increased precision internal data types etc...");
  }
#endif

  confirmParameter( m_log2SaoOffsetScale[CH_L]   > (m_internalBitDepth[CH_L  ]<10?0:(m_internalBitDepth[CH_L  ]-10)), "SaoLumaOffsetBitShift must be in the range of 0 to InternalBitDepth-10, inclusive");
  confirmParameter( m_log2SaoOffsetScale[CH_C] > (m_internalBitDepth[CH_C]<10?0:(m_internalBitDepth[CH_C]-10)), "SaoChromaOffsetBitShift must be in the range of 0 to InternalBitDepthC-10, inclusive");

  confirmParameter( m_internChromaFormat >= NUM_CHROMA_FORMAT,                                  "Intern chroma format must be either 400, 420, 422 or 444" );
  confirmParameter( m_FrameRate <= 0,                                                           "Frame rate must be more than 1" );
  confirmParameter( m_temporalSubsampleRatio < 1,                                               "Temporal subsample rate must be no less than 1" );
  confirmParameter( m_framesToBeEncoded < m_switchPOC,                                          "debug POC out of range" );

  confirmParameter( m_GOPSize < 1 ,                                                             "GOP Size must be greater or equal to 1" );
  confirmParameter( m_GOPSize > 1 &&  m_GOPSize % 2,                                            "GOP Size must be a multiple of 2, if GOP Size is greater than 1" );
  confirmParameter( (m_IntraPeriod > 0 && m_IntraPeriod < m_GOPSize) || m_IntraPeriod == 0,     "Intra period must be more than GOP size, or -1 , not 0" );
  confirmParameter( m_InputQueueSize < m_GOPSize ,                                              "Input queue size must be greater or equal to gop size" );
  confirmParameter( m_MCTF && m_InputQueueSize < m_GOPSize + MCTF_ADD_QUEUE_DELAY ,             "Input queue size must be greater or equal to gop size + N frames for MCTF" );
  confirmParameter( m_DecodingRefreshType < 0 || m_DecodingRefreshType > 3,                     "Decoding Refresh Type must be comprised between 0 and 3 included" );
  confirmParameter( m_QP < -6 * (m_internalBitDepth[CH_L] - 8) || m_QP > MAX_QP,                "QP exceeds supported range (-QpBDOffsety to 63)" );
  for( int comp = 0; comp < 3; comp++)
  {
    confirmParameter( m_loopFilterBetaOffsetDiv2[comp] < -12 || m_loopFilterBetaOffsetDiv2[comp] > 12,          "Loop Filter Beta Offset div. 2 exceeds supported range (-12 to 12)" );
    confirmParameter( m_loopFilterTcOffsetDiv2[comp] < -12 || m_loopFilterTcOffsetDiv2[comp] > 12,              "Loop Filter Tc Offset div. 2 exceeds supported range (-12 to 12)" );
  }
  confirmParameter( m_SearchRange < 0 ,                                                         "Search Range must be more than 0" );
  confirmParameter( m_bipredSearchRange < 0 ,                                                   "Bi-prediction refinement search range must be more than 0" );
  confirmParameter( m_minSearchWindow < 0,                                                      "Minimum motion search window size for the adaptive window ME must be greater than or equal to 0" );

  confirmParameter( m_MCTFFrames.size() != m_MCTFStrengths.size(),        "MCTF parameter list sizes differ");
  confirmParameter( m_MCTFNumLeadFrames  < 0,                             "MCTF number of lead frames must be greater than or equal to 0" );
  confirmParameter( m_MCTFNumTrailFrames < 0,                             "MCTF number of trailing frames must be greater than or equal to 0" );
  confirmParameter( m_MCTFNumLeadFrames  > 0 && ! m_MCTF,                 "MCTF disabled but number of MCTF lead frames is given" );
  confirmParameter( m_MCTFNumTrailFrames > 0 && ! m_MCTF,                 "MCTF disabled but number of MCTF trailing frames is given" );
  confirmParameter( m_MCTFNumTrailFrames > 0 && m_framesToBeEncoded <= 0, "If number of MCTF trailing frames is given, the total number of frames to be encoded has to be set" );

  confirmParameter( m_numWppThreads < 0,                            "NumWppThreads out of range");
  confirmParameter( m_ensureWppBitEqual<0 || m_ensureWppBitEqual>1, "WppBitEqual out of range");
  confirmParameter( m_numWppThreads && m_ensureWppBitEqual == 0,    "NumWppThreads > 0 requires WppBitEqual > 0");

  if (!m_lumaReshapeEnable)
  {
    m_reshapeSignalType = RESHAPE_SIGNAL_NULL;
//    m_intraCMD = 0;
  }
  if (m_lumaReshapeEnable && m_reshapeSignalType == RESHAPE_SIGNAL_PQ)
  {
//    m_intraCMD = 1;
  }
  else if (m_lumaReshapeEnable && (m_reshapeSignalType == RESHAPE_SIGNAL_SDR || m_reshapeSignalType == RESHAPE_SIGNAL_HLG))
  {
//    m_intraCMD = 0;
  }
  else
  {
    m_lumaReshapeEnable = false;
  }
  if (m_lumaReshapeEnable)
  {
    confirmParameter(m_updateCtrl < 0, "Min. LMCS Update Control is 0");
    confirmParameter(m_updateCtrl > 2, "Max. LMCS Update Control is 2");
    confirmParameter(m_adpOption < 0, "Min. LMCS Adaptation Option is 0");
    confirmParameter(m_adpOption > 4, "Max. LMCS Adaptation Option is 4");
    confirmParameter(m_initialCW < 0, "Min. Initial Total Codeword is 0");
    confirmParameter(m_initialCW > 1023, "Max. Initial Total Codeword is 1023");
    if (m_updateCtrl > 0 && m_adpOption > 2) { m_adpOption -= 2; }
  }
  confirmParameter( m_EDO && m_bLoopFilterDisable, "no EDO support with LoopFilter disabled" );

  confirmParameter( m_chromaCbQpOffset < -12,   "Min. Chroma Cb QP Offset is -12" );
  confirmParameter( m_chromaCbQpOffset >  12,   "Max. Chroma Cb QP Offset is  12" );
  confirmParameter( m_chromaCrQpOffset < -12,   "Min. Chroma Cr QP Offset is -12" );
  confirmParameter( m_chromaCrQpOffset >  12,   "Max. Chroma Cr QP Offset is  12" );
  confirmParameter( m_chromaCbQpOffsetDualTree < -12,   "Min. Chroma Cb QP Offset for dual tree is -12" );
  confirmParameter( m_chromaCbQpOffsetDualTree >  12,   "Max. Chroma Cb QP Offset for dual tree is  12" );
  confirmParameter( m_chromaCrQpOffsetDualTree < -12,   "Min. Chroma Cr QP Offset for dual tree is -12" );
  confirmParameter( m_chromaCrQpOffsetDualTree >  12,   "Max. Chroma Cr QP Offset for dual tree is  12" );
  if ( m_JointCbCrMode && (m_internChromaFormat == CHROMA_400) )
  {
    msg( WARNING, "****************************************************************************\n");
    msg( WARNING, "** WARNING: --JointCbCr has been disabled because the chromaFormat is 400 **\n");
    msg( WARNING, "****************************************************************************\n");
    m_JointCbCrMode = false;
  }
  if ( m_JointCbCrMode )
  {
    confirmParameter( m_chromaCbCrQpOffset < -12, "Min. Joint Cb-Cr QP Offset is -12");
    confirmParameter( m_chromaCbCrQpOffset >  12, "Max. Joint Cb-Cr QP Offset is  12");
    confirmParameter( m_chromaCbCrQpOffsetDualTree < -12, "Min. Joint Cb-Cr QP Offset for dual tree is -12");
    confirmParameter( m_chromaCbCrQpOffsetDualTree >  12, "Max. Joint Cb-Cr QP Offset for dual tree is  12");
  }

  if (m_lumaLevelToDeltaQPEnabled)
  {
    CONFIRM_PARAMETER_OR_RETURN(m_usePerceptQPA != 0 && m_usePerceptQPA != 5, "LumaLevelToDeltaQP and PerceptQPA conflict");
    msg( WARNING, "\n using deprecated LumaLevelToDeltaQP to force PerceptQPA mode 5" );
    m_usePerceptQPA = 5; // force QPA mode
  }

  if (m_usePerceptQPA && m_dualITree && (m_internChromaFormat != CHROMA_400) && (m_chromaCbQpOffsetDualTree != 0 || m_chromaCrQpOffsetDualTree != 0 || m_chromaCbCrQpOffsetDualTree != 0))
  {
    msg(WARNING, "***************************************************************************\n");
    msg(WARNING, "** WARNING: chroma QPA on, ignoring nonzero dual-tree chroma QP offsets! **\n");
    msg(WARNING, "***************************************************************************\n");
  }
  if (m_usePerceptQPA && (m_QP <= MAX_QP_PERCEPT_QPA) && (m_CTUSize == 128) && (m_SourceWidth <= 2048) && (m_SourceHeight <= 1280) && (m_usePerceptQPA <= 4))
  {
    m_cuQpDeltaSubdiv = 2;
  }
  if ((m_usePerceptQPA == 2 || m_usePerceptQPA == 4) && (m_internChromaFormat != CHROMA_400) && (m_sliceChromaQpOffsetPeriodicity == 0))
  {
    m_sliceChromaQpOffsetPeriodicity = 1;
  }
  const bool QpaTFI = m_usePerceptQPATempFiltISlice && ((m_usePerceptQPA != 2 && m_usePerceptQPA != 4) || m_IntraPeriod <= 16 || m_GOPSize <= 8);
  confirmParameter( QpaTFI, "invalid combination of PerceptQPATempFiltIPic, PerceptQPA, IntraPeriod, and GOPSize" );

  if (m_usePerceptQPATempFiltISlice && (m_QP > MAX_QP_PERCEPT_QPA || m_IntraPeriod <= m_GOPSize))
  {
    msg(WARNING, "disabling Intra-picture temporal QPA mode due to configuration incompatibility\n");
    m_usePerceptQPATempFiltISlice = false;
  }

  confirmParameter( (m_usePerceptQPA > 0) && (m_cuQpDeltaSubdiv > 2),                                     "MaxCuDQPSubdiv must be 2 or smaller when PerceptQPA is on" );
  if ( m_DecodingRefreshType == 2 )
  {
    confirmParameter( m_IntraPeriod > 0 && m_IntraPeriod <= m_GOPSize ,                                   "Intra period must be larger than GOP size for periodic IDR pictures");
  }
  confirmParameter( m_MaxCodingDepth > MAX_CU_DEPTH,                                                      "MaxPartitionDepth exceeds predefined MAX_CU_DEPTH limit");
  confirmParameter( m_MinQT[0] < 1<<MIN_CU_LOG2,                                                          "Minimum QT size should be larger than or equal to 4");
  confirmParameter( m_MinQT[1] < 1<<MIN_CU_LOG2,                                                          "Minimum QT size should be larger than or equal to 4");
  confirmParameter( m_CTUSize < 32,                                                                       "CTUSize must be greater than or equal to 32");
  confirmParameter( m_CTUSize > 128,                                                                      "CTUSize must be less than or equal to 128");
  confirmParameter( m_CTUSize != 32 && m_CTUSize != 64 && m_CTUSize != 128,                               "CTUSize must be a power of 2 (32, 64, or 128)");
  confirmParameter( m_MaxCodingDepth < 1,                                                                 "MaxPartitionDepth must be greater than zero");
  confirmParameter( (m_CTUSize  >> ( m_MaxCodingDepth - 1 ) ) < 8,                                        "Minimum partition width size should be larger than or equal to 8");
  confirmParameter( (m_SourceWidth  % std::max( 8, int(m_CTUSize  >> ( m_MaxCodingDepth - 1 )))) != 0,    "Resulting coded frame width must be a multiple of Max(8, the minimum CU size)");
  confirmParameter( m_log2MaxTbSize > 6,                                                                  "Log2MaxTbSize must be 6 or smaller." );
  confirmParameter( m_log2MaxTbSize < 5,                                                                  "Log2MaxTbSize must be 5 or greater." );

  bool tileFlag = (m_numTileColumnsMinus1 > 0 || m_numTileRowsMinus1 > 0 );
  confirmParameter( tileFlag && m_entropyCodingSyncEnabled, "Tiles and entropy-coding-sync (Wavefronts) can not be applied together, except in the High Throughput Intra 4:4:4 16 profile");

  confirmParameter( m_SourceWidth  % SPS::getWinUnitX(m_internChromaFormat) != 0, "Picture width must be an integer multiple of the specified chroma subsampling");
  confirmParameter( m_SourceHeight % SPS::getWinUnitY(m_internChromaFormat) != 0, "Picture height must be an integer multiple of the specified chroma subsampling");

  confirmParameter( m_aiPad[0] % SPS::getWinUnitX(m_internChromaFormat) != 0, "Horizontal padding must be an integer multiple of the specified chroma subsampling");
  confirmParameter( m_aiPad[1] % SPS::getWinUnitY(m_internChromaFormat) != 0, "Vertical padding must be an integer multiple of the specified chroma subsampling");

  confirmParameter( m_confWinLeft   % SPS::getWinUnitX(m_internChromaFormat) != 0, "Left conformance window offset must be an integer multiple of the specified chroma subsampling");
  confirmParameter( m_confWinRight  % SPS::getWinUnitX(m_internChromaFormat) != 0, "Right conformance window offset must be an integer multiple of the specified chroma subsampling");
  confirmParameter( m_confWinTop    % SPS::getWinUnitY(m_internChromaFormat) != 0, "Top conformance window offset must be an integer multiple of the specified chroma subsampling");
  confirmParameter( m_confWinBottom % SPS::getWinUnitY(m_internChromaFormat) != 0, "Bottom conformance window offset must be an integer multiple of the specified chroma subsampling");

  confirmParameter( ( m_frameParallel || m_ensureFppBitEqual ) && m_useAMaxBT,            "AMaxBT and frame parallel encoding not supported" );
  confirmParameter( ( m_frameParallel || m_ensureFppBitEqual ) && m_cabacInitPresent,     "CabacInitPresent and frame parallel encoding not supported" );
  confirmParameter( ( m_frameParallel || m_ensureFppBitEqual ) && m_alf,                  "ALF and frame parallel encoding not supported" );
  confirmParameter( ( m_frameParallel || m_ensureFppBitEqual ) && m_saoEncodingRate >= 0, "SaoEncodingRate and frame parallel encoding not supported" );
#if ENABLE_TRACING
  confirmParameter( m_frameParallel && ( m_numFppThreads != 0 && m_numFppThreads != 1 ) && ! m_traceFile.empty(), "Tracing and frame parallel encoding not supported" );
#endif

  // max CU width and height should be power of 2
  uint32_t ui = m_CTUSize;
  while(ui)
  {
    ui >>= 1;
    if( (ui & 1) == 1)
    {
      confirmParameter( ui != 1 , "CTU Size should be 2^n");
    }
  }

  /* if this is an intra-only sequence, ie IntraPeriod=1, don't verify the GOP structure
   * This permits the ability to omit a GOP structure specification */
  if ( m_IntraPeriod == 1 && m_GOPList[0].m_POC == -1 )
  {
    m_GOPList[0] = GOPEntry();
    m_GOPList[0].m_QPFactor = 1;
    m_GOPList[0].m_betaOffsetDiv2 = 0;
    m_GOPList[0].m_tcOffsetDiv2 = 0;
    m_GOPList[0].m_POC = 1;
    m_RPLList0[0] = RPLEntry();
    m_RPLList1[0] = RPLEntry();
    m_RPLList0[0].m_POC = m_RPLList1[0].m_POC = 1;
    m_RPLList0[0].m_numRefPicsActive = 4;
    m_GOPList[0].m_numRefPicsActive[0] = 4;
  }
  else
  {
    // set default RA config
    if( m_GOPSize == 16 && m_GOPList[0].m_POC == -1 && m_GOPList[1].m_POC == -1 )
    {
      for( int i = 0; i < 16; i++ )
      {
        m_GOPList[i] = GOPEntry();
        m_GOPList[i].m_sliceType = 'B';
        m_GOPList[i].m_QPFactor = 1;

        m_GOPList[i].m_numRefPicsActive[0] = 2;
        m_GOPList[i].m_numRefPicsActive[1] = 2;
        m_GOPList[i].m_numRefPics[0] = 2;
        m_GOPList[i].m_numRefPics[1] = 2;
      }
      m_GOPList[0].m_POC  = 16;  m_GOPList[0].m_temporalId  = 0;
      m_GOPList[1].m_POC  =  8;  m_GOPList[1].m_temporalId  = 1;
      m_GOPList[2].m_POC  =  4;  m_GOPList[2].m_temporalId  = 2;
      m_GOPList[3].m_POC  =  2;  m_GOPList[3].m_temporalId  = 3;
      m_GOPList[4].m_POC  =  1;  m_GOPList[4].m_temporalId  = 4;
      m_GOPList[5].m_POC  =  3;  m_GOPList[5].m_temporalId  = 4;
      m_GOPList[6].m_POC  =  6;  m_GOPList[6].m_temporalId  = 3;
      m_GOPList[7].m_POC  =  5;  m_GOPList[7].m_temporalId  = 4;
      m_GOPList[8].m_POC  =  7;  m_GOPList[8].m_temporalId  = 4;
      m_GOPList[9].m_POC  = 12;  m_GOPList[9].m_temporalId  = 2;
      m_GOPList[10].m_POC = 10;  m_GOPList[10].m_temporalId = 3;
      m_GOPList[11].m_POC =  9;  m_GOPList[11].m_temporalId = 4;
      m_GOPList[12].m_POC = 11;  m_GOPList[12].m_temporalId = 4;
      m_GOPList[13].m_POC = 14;  m_GOPList[13].m_temporalId = 3;
      m_GOPList[14].m_POC = 13;  m_GOPList[14].m_temporalId = 4;
      m_GOPList[15].m_POC = 15;  m_GOPList[15].m_temporalId = 4;

      m_GOPList[0].m_numRefPics[0]  = 3;
      m_GOPList[8].m_numRefPics[0]  = 3;
      m_GOPList[12].m_numRefPics[0] = 3;
      m_GOPList[13].m_numRefPics[0] = 3;
      m_GOPList[14].m_numRefPics[0] = 3;
      m_GOPList[15].m_numRefPics[0] = 4;

      m_GOPList[0].m_deltaRefPics[0][0]  = 16; m_GOPList[0].m_deltaRefPics[0][1]  = 32; m_GOPList[0].m_deltaRefPics[0][2]  = 24;
      m_GOPList[1].m_deltaRefPics[0][0]  =  8; m_GOPList[1].m_deltaRefPics[0][1]  = 16;
      m_GOPList[2].m_deltaRefPics[0][0]  =  4; m_GOPList[2].m_deltaRefPics[0][1]  = 12;
      m_GOPList[3].m_deltaRefPics[0][0]  =  2; m_GOPList[3].m_deltaRefPics[0][1]  = 10;
      m_GOPList[4].m_deltaRefPics[0][0]  =  1; m_GOPList[4].m_deltaRefPics[0][1]  = -1;
      m_GOPList[5].m_deltaRefPics[0][0]  =  1; m_GOPList[5].m_deltaRefPics[0][1]  = 3;
      m_GOPList[6].m_deltaRefPics[0][0]  =  2; m_GOPList[6].m_deltaRefPics[0][1]  = 6;
      m_GOPList[7].m_deltaRefPics[0][0]  =  1; m_GOPList[7].m_deltaRefPics[0][1]  = 5;
      m_GOPList[8].m_deltaRefPics[0][0]  =  1; m_GOPList[8].m_deltaRefPics[0][1]  = 3; m_GOPList[8].m_deltaRefPics[0][2]  = 7;
      m_GOPList[9].m_deltaRefPics[0][0]  =  4; m_GOPList[9].m_deltaRefPics[0][1]  = 12;
      m_GOPList[10].m_deltaRefPics[0][0] =  2; m_GOPList[10].m_deltaRefPics[0][1] = 10;
      m_GOPList[11].m_deltaRefPics[0][0] =  1; m_GOPList[11].m_deltaRefPics[0][1] = 9;
      m_GOPList[12].m_deltaRefPics[0][0] =  1; m_GOPList[12].m_deltaRefPics[0][1] = 3; m_GOPList[12].m_deltaRefPics[0][2]  = 11;
      m_GOPList[13].m_deltaRefPics[0][0] =  2; m_GOPList[13].m_deltaRefPics[0][1] = 6; m_GOPList[13].m_deltaRefPics[0][2]  = 14;
      m_GOPList[14].m_deltaRefPics[0][0] =  1; m_GOPList[14].m_deltaRefPics[0][1] = 5; m_GOPList[14].m_deltaRefPics[0][2]  = 13;
      m_GOPList[15].m_deltaRefPics[0][0] =  1; m_GOPList[15].m_deltaRefPics[0][1] = 3; m_GOPList[15].m_deltaRefPics[0][2]  = 7; m_GOPList[15].m_deltaRefPics[0][3]  = 15;

      m_GOPList[3].m_numRefPics[1]  = 3;
      m_GOPList[4].m_numRefPics[1]  = 4;
      m_GOPList[5].m_numRefPics[1]  = 3;
      m_GOPList[7].m_numRefPics[1]  = 3;
      m_GOPList[11].m_numRefPics[1] = 3;

      m_GOPList[0].m_deltaRefPics[1][0]  = 16; m_GOPList[0].m_deltaRefPics[1][1]  =  32;
      m_GOPList[1].m_deltaRefPics[1][0]  = -8; m_GOPList[1].m_deltaRefPics[1][1]  =   8;
      m_GOPList[2].m_deltaRefPics[1][0]  = -4; m_GOPList[2].m_deltaRefPics[1][1]  = -12;
      m_GOPList[3].m_deltaRefPics[1][0]  = -2; m_GOPList[3].m_deltaRefPics[1][1]  =  -6; m_GOPList[3].m_deltaRefPics[1][2]  = -14;
      m_GOPList[4].m_deltaRefPics[1][0]  = -1; m_GOPList[4].m_deltaRefPics[1][1]  =  -3; m_GOPList[4].m_deltaRefPics[1][2]  =  -7;  m_GOPList[4].m_deltaRefPics[1][3]  = -15;
      m_GOPList[5].m_deltaRefPics[1][0]  = -1; m_GOPList[5].m_deltaRefPics[1][1]  =  -5; m_GOPList[5].m_deltaRefPics[1][2]  = -13;
      m_GOPList[6].m_deltaRefPics[1][0]  = -2; m_GOPList[6].m_deltaRefPics[1][1]  =  -10;
      m_GOPList[7].m_deltaRefPics[1][0]  = -1; m_GOPList[7].m_deltaRefPics[1][1]  =  -3; m_GOPList[7].m_deltaRefPics[1][2]  = -11;
      m_GOPList[8].m_deltaRefPics[1][0]  = -1; m_GOPList[8].m_deltaRefPics[1][1]  =  -9;
      m_GOPList[9].m_deltaRefPics[1][0]  = -4; m_GOPList[9].m_deltaRefPics[1][1]  =   4;
      m_GOPList[10].m_deltaRefPics[1][0] = -2; m_GOPList[10].m_deltaRefPics[1][1] =  -6;
      m_GOPList[11].m_deltaRefPics[1][0] = -1; m_GOPList[11].m_deltaRefPics[1][1] =  -3; m_GOPList[11].m_deltaRefPics[1][2]  = -7;
      m_GOPList[12].m_deltaRefPics[1][0] = -1; m_GOPList[12].m_deltaRefPics[1][1] =  -5;
      m_GOPList[13].m_deltaRefPics[1][0] = -2; m_GOPList[13].m_deltaRefPics[1][1] =   2;
      m_GOPList[14].m_deltaRefPics[1][0] = -1; m_GOPList[14].m_deltaRefPics[1][1] =  -3;
      m_GOPList[15].m_deltaRefPics[1][0] = -1; m_GOPList[15].m_deltaRefPics[1][1] =   1;

      for( int i = 0; i < 16; i++ )
      {
        switch( m_GOPList[i].m_temporalId )
        {
        case 0: m_GOPList[i].m_QPOffset   = 1;
                m_GOPList[i].m_QPOffsetModelOffset = 0.0;
                m_GOPList[i].m_QPOffsetModelScale  = 0.0;
        break;
        case 1: m_GOPList[i].m_QPOffset   = 1;
                m_GOPList[i].m_QPOffsetModelOffset = -4.8848;
                m_GOPList[i].m_QPOffsetModelScale  = 0.2061;
        break;
        case 2: m_GOPList[i].m_QPOffset   = 4;
                m_GOPList[i].m_QPOffsetModelOffset = -5.7476;
                m_GOPList[i].m_QPOffsetModelScale  = 0.2286;
        break;
        case 3: m_GOPList[i].m_QPOffset   = 5;
                m_GOPList[i].m_QPOffsetModelOffset = -5.90;
                m_GOPList[i].m_QPOffsetModelScale  = 0.2333;
        break;
        case 4: m_GOPList[i].m_QPOffset   = 6;
                m_GOPList[i].m_QPOffsetModelOffset = -7.1444;
                m_GOPList[i].m_QPOffsetModelScale  = 0.3;
        break;
        default: break;
        }
      }
    }
    else if( m_GOPSize == 32 &&
            ( (m_GOPList[0].m_POC == -1 && m_GOPList[1].m_POC == -1) ||
              (m_GOPList[16].m_POC == -1 && m_GOPList[17].m_POC == -1)
              ) )
    {
      for( int i = 0; i < 32; i++ )
      {
        m_GOPList[i] = GOPEntry();
        m_GOPList[i].m_sliceType = 'B';
        m_GOPList[i].m_QPFactor = 1;

        m_GOPList[i].m_numRefPicsActive[0] = 2;
        m_GOPList[i].m_numRefPicsActive[1] = 2;
        m_GOPList[i].m_numRefPics[0] = 2;
        m_GOPList[i].m_numRefPics[1] = 2;
      }
      m_GOPList[0].m_POC  = 32;   m_GOPList[0].m_temporalId  = 0;
      m_GOPList[1].m_POC  = 16;   m_GOPList[1].m_temporalId  = 1;
      m_GOPList[2].m_POC  =  8;   m_GOPList[2].m_temporalId  = 2;
      m_GOPList[3].m_POC  =  4;   m_GOPList[3].m_temporalId  = 3;
      m_GOPList[4].m_POC  =  2;   m_GOPList[4].m_temporalId  = 4;
      m_GOPList[5].m_POC  =  1;   m_GOPList[5].m_temporalId  = 5;
      m_GOPList[6].m_POC  =  3;   m_GOPList[6].m_temporalId  = 5;
      m_GOPList[7].m_POC  =  6;   m_GOPList[7].m_temporalId  = 4;
      m_GOPList[8].m_POC  =  5;   m_GOPList[8].m_temporalId  = 5;
      m_GOPList[9].m_POC  =  7;   m_GOPList[9].m_temporalId  = 5;
      m_GOPList[10].m_POC = 12;   m_GOPList[10].m_temporalId = 3;
      m_GOPList[11].m_POC = 10;   m_GOPList[11].m_temporalId = 4;
      m_GOPList[12].m_POC =  9;   m_GOPList[12].m_temporalId = 5;
      m_GOPList[13].m_POC = 11;   m_GOPList[13].m_temporalId = 5;
      m_GOPList[14].m_POC = 14;   m_GOPList[14].m_temporalId = 4;
      m_GOPList[15].m_POC = 13;   m_GOPList[15].m_temporalId = 5;

      m_GOPList[16].m_POC  = 15;  m_GOPList[16].m_temporalId  = 5;
      m_GOPList[17].m_POC  = 24;  m_GOPList[17].m_temporalId  = 2;
      m_GOPList[18].m_POC  = 20;  m_GOPList[18].m_temporalId  = 3;
      m_GOPList[19].m_POC  = 18;  m_GOPList[19].m_temporalId  = 4;
      m_GOPList[20].m_POC  = 17;  m_GOPList[20].m_temporalId  = 5;
      m_GOPList[21].m_POC  = 19;  m_GOPList[21].m_temporalId  = 5;
      m_GOPList[22].m_POC  = 22;  m_GOPList[22].m_temporalId  = 4;
      m_GOPList[23].m_POC  = 21;  m_GOPList[23].m_temporalId  = 5;
      m_GOPList[24].m_POC  = 23;  m_GOPList[24].m_temporalId  = 5;
      m_GOPList[25].m_POC  = 28;  m_GOPList[25].m_temporalId  = 3;
      m_GOPList[26].m_POC  = 26;  m_GOPList[26].m_temporalId = 4;
      m_GOPList[27].m_POC  = 25;  m_GOPList[27].m_temporalId = 5;
      m_GOPList[28].m_POC  = 27;  m_GOPList[28].m_temporalId = 5;
      m_GOPList[29].m_POC  = 30;  m_GOPList[29].m_temporalId = 4;
      m_GOPList[30].m_POC  = 29;  m_GOPList[30].m_temporalId = 5;
      m_GOPList[31].m_POC  = 31;  m_GOPList[31].m_temporalId = 5;

      m_GOPList[0].m_numRefPics[0]  = 3;
      m_GOPList[9].m_numRefPics[0]  = 3;
      m_GOPList[13].m_numRefPics[0] = 3;
      m_GOPList[14].m_numRefPics[0] = 3;
      m_GOPList[15].m_numRefPics[0] = 3;
      m_GOPList[16].m_numRefPics[0] = 4;
      m_GOPList[21].m_numRefPics[0] = 3;
      m_GOPList[22].m_numRefPics[0] = 3;
      m_GOPList[23].m_numRefPics[0] = 3;
      m_GOPList[24].m_numRefPics[0] = 4;
      m_GOPList[25].m_numRefPics[0] = 3;
      m_GOPList[26].m_numRefPics[0] = 3;
      m_GOPList[27].m_numRefPics[0] = 3;
      m_GOPList[28].m_numRefPics[0] = 4;
      m_GOPList[29].m_numRefPics[0] = 3;
      m_GOPList[30].m_numRefPics[0] = 3;
      m_GOPList[31].m_numRefPics[0] = 4;

      m_GOPList[0].m_deltaRefPics[0][0]  = 32; m_GOPList[0].m_deltaRefPics[0][1]  = 48; m_GOPList[0].m_deltaRefPics[0][2]  = 64;
      m_GOPList[1].m_deltaRefPics[0][0]  = 16; m_GOPList[1].m_deltaRefPics[0][1]  = 32;
      m_GOPList[2].m_deltaRefPics[0][0]  =  8; m_GOPList[2].m_deltaRefPics[0][1]  = 24;
      m_GOPList[3].m_deltaRefPics[0][0]  =  4; m_GOPList[3].m_deltaRefPics[0][1]  = 20;

      m_GOPList[4].m_deltaRefPics[0][0]  =  2; m_GOPList[4].m_deltaRefPics[0][1]  = 18;
      m_GOPList[5].m_deltaRefPics[0][0]  =  1; m_GOPList[5].m_deltaRefPics[0][1]  = -1;
      m_GOPList[6].m_deltaRefPics[0][0]  =  1; m_GOPList[6].m_deltaRefPics[0][1]  = 3;
      m_GOPList[7].m_deltaRefPics[0][0]  =  2; m_GOPList[7].m_deltaRefPics[0][1]  = 6;

      m_GOPList[8].m_deltaRefPics[0][0]  =  1; m_GOPList[8].m_deltaRefPics[0][1]  = 5;
      m_GOPList[9].m_deltaRefPics[0][0]  =  1; m_GOPList[9].m_deltaRefPics[0][1]  = 3;  m_GOPList[9].m_deltaRefPics[0][2]  = 7;
      m_GOPList[10].m_deltaRefPics[0][0] =  4; m_GOPList[10].m_deltaRefPics[0][1] = 12;
      m_GOPList[11].m_deltaRefPics[0][0] =  2; m_GOPList[11].m_deltaRefPics[0][1] = 10;

      m_GOPList[12].m_deltaRefPics[0][0] =  1; m_GOPList[12].m_deltaRefPics[0][1] = 9;
      m_GOPList[13].m_deltaRefPics[0][0] =  1; m_GOPList[13].m_deltaRefPics[0][1] = 3; m_GOPList[13].m_deltaRefPics[0][2]  = 11;
      m_GOPList[14].m_deltaRefPics[0][0] =  2; m_GOPList[14].m_deltaRefPics[0][1] = 6; m_GOPList[14].m_deltaRefPics[0][2]  = 14;
      m_GOPList[15].m_deltaRefPics[0][0] =  1; m_GOPList[15].m_deltaRefPics[0][1] = 5; m_GOPList[15].m_deltaRefPics[0][2]  = 13;

      m_GOPList[16].m_deltaRefPics[0][0] =  1; m_GOPList[16].m_deltaRefPics[0][1] = 3;  m_GOPList[16].m_deltaRefPics[0][2] = 7; m_GOPList[16].m_deltaRefPics[0][3] = 15;
      m_GOPList[17].m_deltaRefPics[0][0] =  8; m_GOPList[17].m_deltaRefPics[0][1] = 24;
      m_GOPList[18].m_deltaRefPics[0][0] =  4; m_GOPList[18].m_deltaRefPics[0][1] = 20;
      m_GOPList[19].m_deltaRefPics[0][0] =  2; m_GOPList[19].m_deltaRefPics[0][1] = 18;

      m_GOPList[20].m_deltaRefPics[0][0] =  1; m_GOPList[20].m_deltaRefPics[0][1] = 17;
      m_GOPList[21].m_deltaRefPics[0][0] =  1; m_GOPList[21].m_deltaRefPics[0][1] = 3; m_GOPList[21].m_deltaRefPics[0][2]  = 19;
      m_GOPList[22].m_deltaRefPics[0][0] =  2; m_GOPList[22].m_deltaRefPics[0][1] = 6; m_GOPList[22].m_deltaRefPics[0][2]  = 22;
      m_GOPList[23].m_deltaRefPics[0][0] =  1; m_GOPList[23].m_deltaRefPics[0][1] = 5; m_GOPList[23].m_deltaRefPics[0][2]  = 21;

      m_GOPList[24].m_deltaRefPics[0][0] =  1; m_GOPList[24].m_deltaRefPics[0][1] =  3; m_GOPList[24].m_deltaRefPics[0][2] =  7; m_GOPList[24].m_deltaRefPics[0][3] = 23;
      m_GOPList[25].m_deltaRefPics[0][0] =  4; m_GOPList[25].m_deltaRefPics[0][1] = 12; m_GOPList[25].m_deltaRefPics[0][2] = 28;
      m_GOPList[26].m_deltaRefPics[0][0] =  2; m_GOPList[26].m_deltaRefPics[0][1] = 10; m_GOPList[26].m_deltaRefPics[0][2] = 26;
      m_GOPList[27].m_deltaRefPics[0][0] =  1; m_GOPList[27].m_deltaRefPics[0][1] =  9; m_GOPList[27].m_deltaRefPics[0][2] = 25;

      m_GOPList[28].m_deltaRefPics[0][0] =  1; m_GOPList[28].m_deltaRefPics[0][1] =  3; m_GOPList[28].m_deltaRefPics[0][2] = 11; m_GOPList[28].m_deltaRefPics[0][3] = 27;
      m_GOPList[29].m_deltaRefPics[0][0] =  2; m_GOPList[29].m_deltaRefPics[0][1] = 14; m_GOPList[29].m_deltaRefPics[0][2] = 30;
      m_GOPList[30].m_deltaRefPics[0][0] =  1; m_GOPList[30].m_deltaRefPics[0][1] = 13; m_GOPList[30].m_deltaRefPics[0][2] = 29;
      m_GOPList[31].m_deltaRefPics[0][0] =  1; m_GOPList[31].m_deltaRefPics[0][1] =  3; m_GOPList[31].m_deltaRefPics[0][2] = 15; m_GOPList[31].m_deltaRefPics[0][3] = 31;

      m_GOPList[3].m_numRefPics[1]  = 3;
      m_GOPList[4].m_numRefPics[1]  = 4;
      m_GOPList[5].m_numRefPics[1]  = 5;
      m_GOPList[6].m_numRefPics[1]  = 4;
      m_GOPList[7].m_numRefPics[1]  = 3;

      m_GOPList[8].m_numRefPics[1]  = 4;
      m_GOPList[9].m_numRefPics[1]  = 3;
      m_GOPList[10].m_numRefPics[1] = 2;
      m_GOPList[11].m_numRefPics[1] = 3;

      m_GOPList[12].m_numRefPics[1] = 4;
      m_GOPList[13].m_numRefPics[1] = 3;
      m_GOPList[15].m_numRefPics[1] = 3;

      m_GOPList[19].m_numRefPics[1] = 3;

      m_GOPList[20].m_numRefPics[1] = 4;
      m_GOPList[21].m_numRefPics[1] = 3;
      m_GOPList[23].m_numRefPics[1] = 3;

      m_GOPList[27].m_numRefPics[1] = 3;

      m_GOPList[0].m_deltaRefPics[1][0] =  32; m_GOPList[0].m_deltaRefPics[1][1] =  48;
      m_GOPList[1].m_deltaRefPics[1][0] = -16; m_GOPList[1].m_deltaRefPics[1][1] =  16;
      m_GOPList[2].m_deltaRefPics[1][0] =  -8; m_GOPList[2].m_deltaRefPics[1][1] = -24;
      m_GOPList[3].m_deltaRefPics[1][0] =  -4; m_GOPList[3].m_deltaRefPics[1][1] = -12; m_GOPList[3].m_deltaRefPics[1][2] = -28;

      m_GOPList[4].m_deltaRefPics[1][0] = -2; m_GOPList[4].m_deltaRefPics[1][1] = -4; m_GOPList[4].m_deltaRefPics[1][2] = -14;  m_GOPList[4].m_deltaRefPics[1][3] = -30;
      m_GOPList[5].m_deltaRefPics[1][0] = -1; m_GOPList[5].m_deltaRefPics[1][1] = -3; m_GOPList[5].m_deltaRefPics[1][2] = -7; m_GOPList[5].m_deltaRefPics[1][3] = -15; m_GOPList[5].m_deltaRefPics[1][4] = -31;
      m_GOPList[6].m_deltaRefPics[1][0] = -1; m_GOPList[6].m_deltaRefPics[1][1] = -5; m_GOPList[6].m_deltaRefPics[1][2] = -13; m_GOPList[6].m_deltaRefPics[1][3] = -29;
      m_GOPList[7].m_deltaRefPics[1][0] = -2; m_GOPList[7].m_deltaRefPics[1][1] = -10; m_GOPList[7].m_deltaRefPics[1][2] = -26;

      m_GOPList[8].m_deltaRefPics[1][0]  = -1; m_GOPList[8].m_deltaRefPics[1][1]  = -3; m_GOPList[8].m_deltaRefPics[1][2]  = -11; m_GOPList[8].m_deltaRefPics[1][3]  = -27;
      m_GOPList[9].m_deltaRefPics[1][0]  = -1; m_GOPList[9].m_deltaRefPics[1][1]  = -9; m_GOPList[9].m_deltaRefPics[1][2]  = -25;
      m_GOPList[10].m_deltaRefPics[1][0] = -4; m_GOPList[10].m_deltaRefPics[1][1] = -20;
      m_GOPList[11].m_deltaRefPics[1][0] = -2; m_GOPList[11].m_deltaRefPics[1][1] = -6; m_GOPList[11].m_deltaRefPics[1][2]  = -22;

      m_GOPList[12].m_deltaRefPics[1][0] = -1; m_GOPList[12].m_deltaRefPics[1][1] = -3; m_GOPList[12].m_deltaRefPics[1][2] = -7; m_GOPList[12].m_deltaRefPics[1][3] = -23;
      m_GOPList[13].m_deltaRefPics[1][0] = -1; m_GOPList[13].m_deltaRefPics[1][1] = -5; m_GOPList[13].m_deltaRefPics[1][2] = -21;
      m_GOPList[14].m_deltaRefPics[1][0] = -2; m_GOPList[14].m_deltaRefPics[1][1] = -18;
      m_GOPList[15].m_deltaRefPics[1][0] = -1; m_GOPList[15].m_deltaRefPics[1][1] = -3; m_GOPList[15].m_deltaRefPics[1][2] = -19;

      m_GOPList[16].m_deltaRefPics[1][0] = -1; m_GOPList[16].m_deltaRefPics[1][1]  =  -17;
      m_GOPList[17].m_deltaRefPics[1][0] = -8; m_GOPList[17].m_deltaRefPics[1][1]  =   8;
      m_GOPList[18].m_deltaRefPics[1][0] = -4; m_GOPList[18].m_deltaRefPics[1][1]  = -12;
      m_GOPList[19].m_deltaRefPics[1][0] = -2; m_GOPList[19].m_deltaRefPics[1][1]  =  -6; m_GOPList[19].m_deltaRefPics[1][2]  = -14;

      m_GOPList[20].m_deltaRefPics[1][0] = -1; m_GOPList[20].m_deltaRefPics[1][1]  =  -3; m_GOPList[20].m_deltaRefPics[1][2]  =  -7;  m_GOPList[20].m_deltaRefPics[1][3]  = -15;
      m_GOPList[21].m_deltaRefPics[1][0] = -1; m_GOPList[21].m_deltaRefPics[1][1]  =  -5; m_GOPList[21].m_deltaRefPics[1][2]  = -13;
      m_GOPList[22].m_deltaRefPics[1][0] = -2; m_GOPList[22].m_deltaRefPics[1][1]  =  -10;
      m_GOPList[23].m_deltaRefPics[1][0] = -1; m_GOPList[23].m_deltaRefPics[1][1]  =  -3; m_GOPList[23].m_deltaRefPics[1][2]  = -11;

      m_GOPList[24].m_deltaRefPics[1][0] = -1; m_GOPList[24].m_deltaRefPics[1][1]  =  -9;
      m_GOPList[25].m_deltaRefPics[1][0] = -4; m_GOPList[25].m_deltaRefPics[1][1]  =   4;
      m_GOPList[26].m_deltaRefPics[1][0] = -2; m_GOPList[26].m_deltaRefPics[1][1] =  -6;
      m_GOPList[27].m_deltaRefPics[1][0] = -1; m_GOPList[27].m_deltaRefPics[1][1] =  -3; m_GOPList[27].m_deltaRefPics[1][2]  = -7;

      m_GOPList[28].m_deltaRefPics[1][0] = -1; m_GOPList[28].m_deltaRefPics[1][1] =  -5;
      m_GOPList[29].m_deltaRefPics[1][0] = -2; m_GOPList[29].m_deltaRefPics[1][1] =   2;
      m_GOPList[30].m_deltaRefPics[1][0] = -1; m_GOPList[30].m_deltaRefPics[1][1] =  -3;
      m_GOPList[31].m_deltaRefPics[1][0] = -1; m_GOPList[31].m_deltaRefPics[1][1] =   1;

      for( int i = 0; i < 32; i++ )
      {
        switch( m_GOPList[i].m_temporalId )
        {
        case 0: m_GOPList[i].m_QPOffset   = -1;
                m_GOPList[i].m_QPOffsetModelOffset = 0.0;
                m_GOPList[i].m_QPOffsetModelScale  = 0.0;
                break;
        case 1: m_GOPList[i].m_QPOffset   = 0;
                m_GOPList[i].m_QPOffsetModelOffset = -4.9309;
                m_GOPList[i].m_QPOffsetModelScale  =  0.2265;
                break;
        case 2: m_GOPList[i].m_QPOffset   = 1;
                m_GOPList[i].m_QPOffsetModelOffset = -5.4095;
                m_GOPList[i].m_QPOffsetModelScale  =  0.2353;
                break;
        case 3: m_GOPList[i].m_QPOffset   = 3;
                m_GOPList[i].m_QPOffsetModelOffset = -5.4095;
                m_GOPList[i].m_QPOffsetModelScale  =  0.2571;
                break;
        case 4: m_GOPList[i].m_QPOffset   = 5;
                m_GOPList[i].m_QPOffsetModelOffset = -4.4895;
                m_GOPList[i].m_QPOffsetModelScale  =  0.1947;
                break;
        case 5: m_GOPList[i].m_QPOffset   = 6;
                m_GOPList[i].m_QPOffsetModelOffset = -5.4429;
                m_GOPList[i].m_QPOffsetModelScale  =  0.2429;
                break;
        default: break;
        }
      }
    }

    confirmParameter( m_intraOnlyConstraintFlag, "IntraOnlyConstraintFlag cannot be 1 for inter sequences");
  }

  for (int i = 0; m_GOPList[i].m_POC != -1 && i < MAX_GOP + 1; i++)
  {
    m_RPLList0[i].m_POC = m_RPLList1[i].m_POC = m_GOPList[i].m_POC;
    m_RPLList0[i].m_temporalId = m_RPLList1[i].m_temporalId = m_GOPList[i].m_temporalId;
    m_RPLList0[i].m_refPic = m_RPLList1[i].m_refPic = m_GOPList[i].m_refPic;
    m_RPLList0[i].m_sliceType = m_RPLList1[i].m_sliceType = m_GOPList[i].m_sliceType;

    m_RPLList0[i].m_numRefPicsActive = m_GOPList[i].m_numRefPicsActive[0];
    m_RPLList1[i].m_numRefPicsActive = m_GOPList[i].m_numRefPicsActive[1];
    m_RPLList0[i].m_numRefPics = m_GOPList[i].m_numRefPics[0];
    m_RPLList1[i].m_numRefPics = m_GOPList[i].m_numRefPics[1];
    m_RPLList0[i].m_ltrp_in_slice_header_flag = m_GOPList[i].m_ltrp_in_slice_header_flag;
    m_RPLList1[i].m_ltrp_in_slice_header_flag = m_GOPList[i].m_ltrp_in_slice_header_flag;

    for (int j = 0; j < m_GOPList[i].m_numRefPics[0]; j++)
      m_RPLList0[i].m_deltaRefPics[j] = m_GOPList[i].m_deltaRefPics[0][j];
    for (int j = 0; j < m_GOPList[i].m_numRefPics[1]; j++)
      m_RPLList1[i].m_deltaRefPics[j] = m_GOPList[i].m_deltaRefPics[1][j];
  }

  int multipleFactor = /*m_compositeRefEnabled ? 2 :*/ 1;
  bool verifiedGOP=false;
  bool errorGOP=false;
  int checkGOP=1;
  int refList[MAX_NUM_REF_PICS+1] = {0};
  bool isOK[MAX_GOP];
  for(int i=0; i<MAX_GOP; i++)
  {
    isOK[i]=false;
  }
  int numOK=0;
  confirmParameter( m_IntraPeriod >=0&&(m_IntraPeriod%m_GOPSize!=0), "Intra period must be a multiple of GOPSize, or -1" );
  confirmParameter( m_temporalSubsampleRatio < 1, "TemporalSubsampleRatio must be greater than 0");

  for(int i=0; i<m_GOPSize; i++)
  {
    if (m_GOPList[i].m_POC == m_GOPSize * multipleFactor)
    {
      confirmParameter( m_GOPList[i].m_temporalId!=0 , "The last frame in each GOP must have temporal ID = 0 " );
    }
  }

  if ( (m_IntraPeriod != 1) && !m_loopFilterOffsetInPPS && (!m_bLoopFilterDisable) )
  {
    for(int i=0; i<m_GOPSize; i++)
    {
      for( int comp = 0; comp < 3; comp++ )
      {
        confirmParameter( (m_GOPList[i].m_betaOffsetDiv2 + m_loopFilterBetaOffsetDiv2[comp]) < -12 || (m_GOPList[i].m_betaOffsetDiv2 + m_loopFilterBetaOffsetDiv2[comp]) > 12, "Loop Filter Beta Offset div. 2 for one of the GOP entries exceeds supported range (-12 to 12)" );
        confirmParameter( (m_GOPList[i].m_tcOffsetDiv2 + m_loopFilterTcOffsetDiv2[comp]) < -12 || (m_GOPList[i].m_tcOffsetDiv2 + m_loopFilterTcOffsetDiv2[comp]) > 12, "Loop Filter Tc Offset div. 2 for one of the GOP entries exceeds supported range (-12 to 12)" );
      }
    }
  }

  for(int i=0; i<m_GOPSize; i++)
  {
    confirmParameter( abs(m_GOPList[i].m_CbQPoffset               ) > 12, "Cb QP Offset for one of the GOP entries exceeds supported range (-12 to 12)" );
    confirmParameter( abs(m_GOPList[i].m_CbQPoffset + m_chromaCbQpOffset) > 12, "Cb QP Offset for one of the GOP entries, when combined with the PPS Cb offset, exceeds supported range (-12 to 12)" );
    confirmParameter( abs(m_GOPList[i].m_CrQPoffset               ) > 12, "Cr QP Offset for one of the GOP entries exceeds supported range (-12 to 12)" );
    confirmParameter( abs(m_GOPList[i].m_CrQPoffset + m_chromaCrQpOffset) > 12, "Cr QP Offset for one of the GOP entries, when combined with the PPS Cr offset, exceeds supported range (-12 to 12)" );
  }
  confirmParameter( abs(m_sliceChromaQpOffsetIntraOrPeriodic[0]                 ) > 12, "Intra/periodic Cb QP Offset exceeds supported range (-12 to 12)" );
  confirmParameter( abs(m_sliceChromaQpOffsetIntraOrPeriodic[0]  + m_chromaCbQpOffset ) > 12, "Intra/periodic Cb QP Offset, when combined with the PPS Cb offset, exceeds supported range (-12 to 12)" );
  confirmParameter( abs(m_sliceChromaQpOffsetIntraOrPeriodic[1]                 ) > 12, "Intra/periodic Cr QP Offset exceeds supported range (-12 to 12)" );
  confirmParameter( abs(m_sliceChromaQpOffsetIntraOrPeriodic[1]  + m_chromaCrQpOffset ) > 12, "Intra/periodic Cr QP Offset, when combined with the PPS Cr offset, exceeds supported range (-12 to 12)" );

  confirmParameter( m_fastLocalDualTreeMode < 0 || m_fastLocalDualTreeMode > 2, "FastLocalDualTreeMode must be in range [0..2]" );

  int extraRPLs = 0;
  int numRefs   = 1;
  //start looping through frames in coding order until we can verify that the GOP structure is correct.
  while (!verifiedGOP && !errorGOP)
  {
    int curGOP = (checkGOP - 1) % m_GOPSize;
    int curPOC = ((checkGOP - 1) / m_GOPSize)*m_GOPSize * multipleFactor + m_RPLList0[curGOP].m_POC;
    if (m_RPLList0[curGOP].m_POC < 0 || m_RPLList1[curGOP].m_POC < 0)
    {
      msg(WARNING, "\nError: found fewer Reference Picture Sets than GOPSize\n");
      errorGOP = true;
    }
    else
    {
      //check that all reference pictures are available, or have a POC < 0 meaning they might be available in the next GOP.
      bool beforeI = false;
      for (int i = 0; i< m_RPLList0[curGOP].m_numRefPics; i++)
      {
        int absPOC = curPOC - m_RPLList0[curGOP].m_deltaRefPics[i];
        if (absPOC < 0)
        {
          beforeI = true;
        }
        else
        {
          bool found = false;
          for (int j = 0; j<numRefs; j++)
          {
            if (refList[j] == absPOC)
            {
              found = true;
              for (int k = 0; k<m_GOPSize; k++)
              {
                if (absPOC % (m_GOPSize * multipleFactor) == m_RPLList0[k].m_POC % (m_GOPSize * multipleFactor))
                {
                  if (m_RPLList0[k].m_temporalId == m_RPLList0[curGOP].m_temporalId)
                  {
                    m_RPLList0[k].m_refPic = true;
                  }
                }
              }
            }
          }
          if (!found)
          {
            msg(WARNING, "\nError: ref pic %d is not available for GOP frame %d\n", m_RPLList0[curGOP].m_deltaRefPics[i], curGOP + 1);
            errorGOP = true;
          }
        }
      }
      if (!beforeI && !errorGOP)
      {
        //all ref frames were present
        if (!isOK[curGOP])
        {
          numOK++;
          isOK[curGOP] = true;
          if (numOK == m_GOPSize)
          {
            verifiedGOP = true;
          }
        }
      }
      else
      {
        //create a new RPLEntry for this frame containing all the reference pictures that were available (POC > 0)
        m_RPLList0[m_GOPSize + extraRPLs] = m_RPLList0[curGOP];
        m_RPLList1[m_GOPSize + extraRPLs] = m_RPLList1[curGOP];
        int newRefs0 = 0;
        for (int i = 0; i< m_RPLList0[curGOP].m_numRefPics; i++)
        {
          int absPOC = curPOC - m_RPLList0[curGOP].m_deltaRefPics[i];
          if (absPOC >= 0)
          {
            m_RPLList0[m_GOPSize + extraRPLs].m_deltaRefPics[newRefs0] = m_RPLList0[curGOP].m_deltaRefPics[i];
            newRefs0++;
          }
        }
        int numPrefRefs0 = m_RPLList0[curGOP].m_numRefPicsActive;

        int newRefs1 = 0;
        for (int i = 0; i< m_RPLList1[curGOP].m_numRefPics; i++)
        {
          int absPOC = curPOC - m_RPLList1[curGOP].m_deltaRefPics[i];
          if (absPOC >= 0)
          {
            m_RPLList1[m_GOPSize + extraRPLs].m_deltaRefPics[newRefs1] = m_RPLList1[curGOP].m_deltaRefPics[i];
            newRefs1++;
          }
        }
        int numPrefRefs1 = m_RPLList1[curGOP].m_numRefPicsActive;

        for (int offset = -1; offset>-checkGOP; offset--)
        {
          //step backwards in coding order and include any extra available pictures we might find useful to replace the ones with POC < 0.
          int offGOP = (checkGOP - 1 + offset) % m_GOPSize;
          int offPOC = ((checkGOP - 1 + offset) / m_GOPSize)*(m_GOPSize * multipleFactor) + m_RPLList0[offGOP].m_POC;
          if (offPOC >= 0 && m_RPLList0[offGOP].m_temporalId <= m_RPLList0[curGOP].m_temporalId)
          {
            bool newRef = false;
            for (int i = 0; i<(newRefs0 + newRefs1); i++)
            {
              if (refList[i] == offPOC)
              {
                newRef = true;
              }
            }
            for (int i = 0; i<newRefs0; i++)
            {
              if (m_RPLList0[m_GOPSize + extraRPLs].m_deltaRefPics[i] == curPOC - offPOC)
              {
                newRef = false;
              }
            }
            if (newRef)
            {
              int insertPoint = newRefs0;
              //this picture can be added, find appropriate place in list and insert it.
              if (m_RPLList0[offGOP].m_temporalId == m_RPLList0[curGOP].m_temporalId)
              {
                m_RPLList0[offGOP].m_refPic = true;
              }
              for (int j = 0; j<newRefs0; j++)
              {
                if (m_RPLList0[m_GOPSize + extraRPLs].m_deltaRefPics[j] > curPOC - offPOC && curPOC - offPOC > 0)
                {
                  insertPoint = j;
                  break;
                }
              }
              int prev = curPOC - offPOC;
              for (int j = insertPoint; j<newRefs0 + 1; j++)
              {
                int newPrev = m_RPLList0[m_GOPSize + extraRPLs].m_deltaRefPics[j];
                m_RPLList0[m_GOPSize + extraRPLs].m_deltaRefPics[j] = prev;
                prev = newPrev;
              }
              newRefs0++;
            }
          }
          if (newRefs0 >= numPrefRefs0)
          {
            break;
          }
        }

        for (int offset = -1; offset>-checkGOP; offset--)
        {
          //step backwards in coding order and include any extra available pictures we might find useful to replace the ones with POC < 0.
          int offGOP = (checkGOP - 1 + offset) % m_GOPSize;
          int offPOC = ((checkGOP - 1 + offset) / m_GOPSize)*(m_GOPSize * multipleFactor) + m_RPLList1[offGOP].m_POC;
          if (offPOC >= 0 && m_RPLList1[offGOP].m_temporalId <= m_RPLList1[curGOP].m_temporalId)
          {
            bool newRef = false;
            for (int i = 0; i<(newRefs0 + newRefs1); i++)
            {
              if (refList[i] == offPOC)
              {
                newRef = true;
              }
            }
            for (int i = 0; i<newRefs1; i++)
            {
              if (m_RPLList1[m_GOPSize + extraRPLs].m_deltaRefPics[i] == curPOC - offPOC)
              {
                newRef = false;
              }
            }
            if (newRef)
            {
              int insertPoint = newRefs1;
              //this picture can be added, find appropriate place in list and insert it.
              if (m_RPLList1[offGOP].m_temporalId == m_RPLList1[curGOP].m_temporalId)
              {
                m_RPLList1[offGOP].m_refPic = true;
              }
              for (int j = 0; j<newRefs1; j++)
              {
                if (m_RPLList1[m_GOPSize + extraRPLs].m_deltaRefPics[j] > curPOC - offPOC && curPOC - offPOC > 0)
                {
                  insertPoint = j;
                  break;
                }
              }
              int prev = curPOC - offPOC;
              for (int j = insertPoint; j<newRefs1 + 1; j++)
              {
                int newPrev = m_RPLList1[m_GOPSize + extraRPLs].m_deltaRefPics[j];
                m_RPLList1[m_GOPSize + extraRPLs].m_deltaRefPics[j] = prev;
                prev = newPrev;
              }
              newRefs1++;
            }
          }
          if (newRefs1 >= numPrefRefs1)
          {
            break;
          }
        }

        m_RPLList0[m_GOPSize + extraRPLs].m_numRefPics = newRefs0;
        m_RPLList0[m_GOPSize + extraRPLs].m_numRefPicsActive = std::min(m_RPLList0[m_GOPSize + extraRPLs].m_numRefPics, m_RPLList0[m_GOPSize + extraRPLs].m_numRefPicsActive);
        m_RPLList1[m_GOPSize + extraRPLs].m_numRefPics = newRefs1;
        m_RPLList1[m_GOPSize + extraRPLs].m_numRefPicsActive = std::min(m_RPLList1[m_GOPSize + extraRPLs].m_numRefPics, m_RPLList1[m_GOPSize + extraRPLs].m_numRefPicsActive);
        curGOP = m_GOPSize + extraRPLs;
        extraRPLs++;
      }
      numRefs = 0;
      for (int i = 0; i< m_RPLList0[curGOP].m_numRefPics; i++)
      {
        int absPOC = curPOC - m_RPLList0[curGOP].m_deltaRefPics[i];
        if (absPOC >= 0)
        {
          refList[numRefs] = absPOC;
          numRefs++;
        }
      }
      for (int i = 0; i< m_RPLList1[curGOP].m_numRefPics; i++)
      {
        int absPOC = curPOC - m_RPLList1[curGOP].m_deltaRefPics[i];
        if (absPOC >= 0)
        {
          bool alreadyExist = false;
          for (int j = 0; !alreadyExist && j < numRefs; j++)
          {
            if (refList[j] == absPOC)
            {
              alreadyExist = true;
            }
          }
          if (!alreadyExist)
          {
            refList[numRefs] = absPOC;
            numRefs++;
          }
        }
      }
      refList[numRefs] = curPOC;
      numRefs++;
    }
    checkGOP++;
  }
  confirmParameter(errorGOP, "Invalid GOP structure given");

  m_maxTempLayer = 1;

  for(int i=0; i<m_GOPSize; i++)
  {
    if(m_GOPList[i].m_temporalId >= m_maxTempLayer)
    {
      m_maxTempLayer = m_GOPList[i].m_temporalId+1;
    }
    confirmParameter(m_GOPList[i].m_sliceType!='B' && m_GOPList[i].m_sliceType!='P' && m_GOPList[i].m_sliceType!='I', "Slice type must be equal to B or P or I");
  }
  for(int i=0; i<MAX_TLAYER; i++)
  {
    m_numReorderPics[i] = 0;
    m_maxDecPicBuffering[i] = 1;
  }
  for(int i=0; i<m_GOPSize; i++)
  {
    int numRefPic = m_RPLList0[i].m_numRefPics;
    for (int tmp = 0; tmp < m_RPLList1[i].m_numRefPics; tmp++)
    {
      bool notSame = true;
      for (int jj = 0; notSame && jj < m_RPLList0[i].m_numRefPics; jj++)
      {
        if (m_RPLList1[i].m_deltaRefPics[tmp] == m_RPLList0[i].m_deltaRefPics[jj]) notSame = false;
      }
      if (notSame) numRefPic++;
    }
    if (numRefPic + 1 > m_maxDecPicBuffering[m_GOPList[i].m_temporalId])
    {
      m_maxDecPicBuffering[m_GOPList[i].m_temporalId] = numRefPic + 1;
    }
    int highestDecodingNumberWithLowerPOC = 0;
    for(int j=0; j<m_GOPSize; j++)
    {
      if(m_GOPList[j].m_POC <= m_GOPList[i].m_POC)
      {
        highestDecodingNumberWithLowerPOC = j;
      }
    }
    int numReorder = 0;
    for(int j=0; j<highestDecodingNumberWithLowerPOC; j++)
    {
      if(m_GOPList[j].m_temporalId <= m_GOPList[i].m_temporalId &&
          m_GOPList[j].m_POC > m_GOPList[i].m_POC)
      {
        numReorder++;
      }
    }
    if(numReorder > m_numReorderPics[m_GOPList[i].m_temporalId])
    {
      m_numReorderPics[m_GOPList[i].m_temporalId] = numReorder;
    }
  }

  for(int i=0; i<MAX_TLAYER-1; i++)
  {
    // a lower layer can not have higher value of m_numReorderPics than a higher layer
    if(m_numReorderPics[i+1] < m_numReorderPics[i])
    {
      m_numReorderPics[i+1] = m_numReorderPics[i];
    }
    // the value of num_reorder_pics[ i ] shall be in the range of 0 to max_dec_pic_buffering[ i ] - 1, inclusive
    if(m_numReorderPics[i] > m_maxDecPicBuffering[i] - 1)
    {
      m_maxDecPicBuffering[i] = m_numReorderPics[i] + 1;
    }
    // a lower layer can not have higher value of m_uiMaxDecPicBuffering than a higher layer
    if(m_maxDecPicBuffering[i+1] < m_maxDecPicBuffering[i])
    {
      m_maxDecPicBuffering[i+1] = m_maxDecPicBuffering[i];
    }
  }

  // the value of num_reorder_pics[ i ] shall be in the range of 0 to max_dec_pic_buffering[ i ] -  1, inclusive
  if(m_numReorderPics[MAX_TLAYER-1] > m_maxDecPicBuffering[MAX_TLAYER-1] - 1)
  {
    m_maxDecPicBuffering[MAX_TLAYER-1] = m_numReorderPics[MAX_TLAYER-1] + 1;
  }

  int   iWidthInCU  = ( m_SourceWidth%m_CTUSize )  ? m_SourceWidth/m_CTUSize  + 1 : m_SourceWidth/m_CTUSize;
  int   iHeightInCU = ( m_SourceHeight%m_CTUSize ) ? m_SourceHeight/m_CTUSize + 1 : m_SourceHeight/m_CTUSize;
  uint32_t  uiCummulativeColumnWidth = 0;
  uint32_t  uiCummulativeRowHeight   = 0;

  //check the column relative parameters
  confirmParameter( m_numTileColumnsMinus1 >= (1<<(LOG2_MAX_NUM_COLUMNS_MINUS1+1)), "The number of columns is larger than the maximum allowed number of columns." );
  confirmParameter( m_numTileColumnsMinus1 >= iWidthInCU,                           "The current picture can not have so many columns." );
  if( m_numTileColumnsMinus1 && !m_tileUniformSpacingFlag )
  {
    for( int i=0; i<m_numTileColumnsMinus1; i++ )
    {
      uiCummulativeColumnWidth += m_tileColumnWidth[i];
    }
    confirmParameter( uiCummulativeColumnWidth >= iWidthInCU, "The width of the column is too large." );
  }

  //check the row relative parameters
  confirmParameter( m_numTileRowsMinus1 >= (1<<(LOG2_MAX_NUM_ROWS_MINUS1+1)), "The number of rows is larger than the maximum allowed number of rows." );
  confirmParameter( m_numTileRowsMinus1 >= iHeightInCU,                       "The current picture can not have so many rows." );
  if( m_numTileRowsMinus1 && !m_tileUniformSpacingFlag )
  {
    for(int i=0; i<m_numTileRowsMinus1; i++)
    {
      uiCummulativeRowHeight += m_tileRowHeight[i];
    }
    confirmParameter( uiCummulativeRowHeight >= iHeightInCU, "The height of the row is too large." );
  }

  confirmParameter( m_MCTF > 2 || m_MCTF < 0, "MCTF out of range" );

  if( m_MCTF && m_QP < 17 )
  {
    msg( WARNING, "disable MCTF for QP < 17\n");
    m_MCTF = 0;
  }

  if( m_MCTF )
  {
    if( m_MCTFFrames.empty() )
    {
      if( m_GOPSize == 32 )
      {
        m_MCTFStrengths.push_back(0.28125); //  9/32
        m_MCTFFrames.push_back(8);
        m_MCTFStrengths.push_back(0.5625);  // 18/32
        m_MCTFFrames.push_back(16);
        m_MCTFStrengths.push_back(0.84375); // 27/32
        m_MCTFFrames.push_back(32);
      }
      else if( m_GOPSize == 16 )
      {
        m_MCTFStrengths.push_back(0.4);
        m_MCTFFrames.push_back(8);
        m_MCTFStrengths.push_back(0.8);
        m_MCTFFrames.push_back(16);
      }
      else if( m_GOPSize == 8 )
      {
        m_MCTFStrengths.push_back(0.65625); // 21/32
        m_MCTFFrames.push_back(8);
      }
      else
      {
        msg( WARNING, "no MCTF frames selected, MCTF will be inactive!\n");
      }
    }

    confirmParameter( m_MCTFFrames.size() != m_MCTFStrengths.size(), "MCTFFrames and MCTFStrengths do not match");
  }

  if ( ! m_MMVD && m_allowDisFracMMVD )
  {
    msg( WARNING, "MMVD disabled, thus disable AllowDisFracMMVD too\n" );
    m_allowDisFracMMVD = false;
  }

  if( m_fastForwardToPOC != -1 )
  {
    if( m_cabacInitPresent )  { msg( WARNING, "WARNING usage of FastForwardToPOC and CabacInitPresent might cause different behaviour\n\n" ); }
    if( m_alf )               { msg( WARNING, "WARNING usage of FastForwardToPOC and ALF might cause different behaviour\n\n" ); }
  }

  //
  // finalize initialization
  //


  // coding structure
  m_numRPLList0 = 0;
  for ( int i = 0; i < MAX_GOP; i++ )
  {
    if ( m_RPLList0[ i ].m_POC != -1 )
      m_numRPLList0++;
  }
  m_numRPLList1 = 0;
  for ( int i = 0; i < MAX_GOP; i++ )
  {
    if ( m_RPLList1[ i ].m_POC != -1 )
      m_numRPLList1++;
  }

  int tilesCount = ( m_numTileRowsMinus1 + 1 ) * ( m_numTileColumnsMinus1 + 1 );
  if ( tilesCount == 1 )
  {
    m_bLFCrossTileBoundaryFlag = true;
  }

  m_PROF &= bool(m_Affine);
  if (m_Affine > 1)
  {
    m_PROF = bool(m_Affine);
    m_AffineType = (m_Affine == 2) ? true : false;
  }

  if ( m_frameParallel )
  {
    if ( m_numFppThreads < 0 )
    {
      const int iqs = m_MCTF ? m_InputQueueSize - MCTF_ADD_QUEUE_DELAY : m_InputQueueSize;
            int num = m_GOPSize;
      m_numFppThreads = 0;
      for ( int i = 0; i < iqs; i += m_GOPSize )
      {
        num = num > 1 ? num >> 1 : 1;
        m_numFppThreads += num;
      }
      confirmParameter( m_numFppThreads > m_InputQueueSize, "number of frame parallel processing threads should be less then size of input queue" );
    }
  }

  return( m_confirmFailed );
}

void EncCfg::setCfgParameter( const EncCfg& encCfg )
{
  *this = encCfg;
}

} // namespace vvenc

//! \}

