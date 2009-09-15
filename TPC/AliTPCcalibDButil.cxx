/**************************************************************************
 * Copyright(c) 1998-1999, ALICE Experiment at CERN, All rights reserved. *
 *                                                                        *
 * Author: The ALICE Off-line Project.                                    *
 * Contributors are mentioned in the code where appropriate.              *
 *                                                                        *
 * Permission to use, copy, modify and distribute this software and its   *
 * documentation strictly for non-commercial purposes is hereby granted   *
 * without fee, provided that the above copyright notice appears in all   *
 * copies and that both the copyright notice and this permission notice   *
 * appear in the supporting documentation. The authors make no claims     *
 * about the suitability of this software for any purpose. It is          *
 * provided "as is" without express or implied warranty.                  *
 **************************************************************************/


///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// Class providing the calculation of derived quantities (mean,rms,fits,...) //
//       of calibration entries                                              //
/*


*/
////////////////////////////////////////////////////////////////////////////////

#include <TMath.h>
#include <TVectorT.h>
#include <TObjArray.h>
#include <TGraph.h>

#include <AliDCSSensorArray.h>
#include <AliDCSSensor.h>
#include "AliTPCcalibDB.h"
#include "AliTPCCalPad.h"
#include "AliTPCCalROC.h"
#include "AliTPCROC.h"
#include "AliTPCmapper.h"
#include "AliTPCParam.h"
#include "AliTPCCalibRaw.h"

#include "AliTPCcalibDButil.h"

ClassImp(AliTPCcalibDButil)
AliTPCcalibDButil::AliTPCcalibDButil() :
  TObject(),
  fCalibDB(AliTPCcalibDB::Instance()),
  fPadNoise(0x0),
  fPedestals(0x0),
  fPulserTmean(0x0),
  fPulserTrms(0x0),
  fPulserQmean(0x0),
  fPulserOutlier(new AliTPCCalPad("PulserOutliers","PulserOutliers")),
  fCETmean(0x0),
  fCETrms(0x0),
  fCEQmean(0x0),
  fALTROMasked(0x0),
  fCalibRaw(0x0),
  fGoofieArray(0x0),
  fMapper(new AliTPCmapper(0x0)),
  fNpulserOutliers(-1),
  fIrocTimeOffset(.2),
  fCETmaxLimitAbs(1.5),
  fPulTmaxLimitAbs(1.5),
  fPulQmaxLimitAbs(5),
  fPulQminLimit(11)
{
  //
  // Default ctor
  //
}
//_____________________________________________________________________________________
AliTPCcalibDButil::~AliTPCcalibDButil()
{
  //
  // dtor
  //
  delete fPulserOutlier;
  delete fMapper;
}
//_____________________________________________________________________________________
void AliTPCcalibDButil::UpdateFromCalibDB()
{
  //
  // Update pointers from calibDB
  //
  fPadNoise=fCalibDB->GetPadNoise();
  fPedestals=fCalibDB->GetPedestals();
  fPulserTmean=fCalibDB->GetPulserTmean();
  fPulserTrms=fCalibDB->GetPulserTrms();
  fPulserQmean=fCalibDB->GetPulserQmean();
  fCETmean=fCalibDB->GetCETmean();
  fCETrms=fCalibDB->GetCETrms();
  fCEQmean=fCalibDB->GetCEQmean();
  fALTROMasked=fCalibDB->GetALTROMasked();
  fGoofieArray=fCalibDB->GetGoofieSensors(fCalibDB->GetRun());
  fCalibRaw=fCalibDB->GetCalibRaw();
  UpdatePulserOutlierMap();
}
//_____________________________________________________________________________________
void AliTPCcalibDButil::ProcessCEdata(const char* fitFormula, TVectorD &fitResultsA, TVectorD &fitResultsC, Int_t &noutliersCE)
{
  //
  // Process the CE data for this run
  // the return TVectorD arrays contian the results of the fit
  // noutliersCE contains the number of pads marked as outliers,
  //   not including masked and edge pads
  //
  
  //retrieve CE and ALTRO data
  if (!fCETmean){
    TString fitString(fitFormula);
    fitString.ReplaceAll("++","#");
    Int_t ndim=fitString.CountChar('#')+2;
    fitResultsA.ResizeTo(ndim);
    fitResultsC.ResizeTo(ndim);
    fitResultsA.Zero();
    fitResultsC.Zero();
    noutliersCE=-1;
    return;
  }
  noutliersCE=0;
  //create outlier map
  AliTPCCalPad out("out","out");
  AliTPCCalROC *rocMasked=0x0;
  //loop over all channels
  for (UInt_t iroc=0;iroc<fCETmean->kNsec;++iroc){
    AliTPCCalROC *rocData=fCETmean->GetCalROC(iroc);
    if (fALTROMasked) rocMasked=fALTROMasked->GetCalROC(iroc);
    AliTPCCalROC *rocOut=out.GetCalROC(iroc);
    if (!rocData) {
      noutliersCE+=AliTPCROC::Instance()->GetNChannels(iroc);
      continue;
    }
    //add time offset to IROCs
    if (iroc<AliTPCROC::Instance()->GetNInnerSector())
      rocData->Add(fIrocTimeOffset);
    //select outliers
    UInt_t nrows=rocData->GetNrows();
    for (UInt_t irow=0;irow<nrows;++irow){
      UInt_t npads=rocData->GetNPads(irow);
      for (UInt_t ipad=0;ipad<npads;++ipad){
        //exclude masked pads
        if (rocMasked && rocMasked->GetValue(irow,ipad)) {
          rocOut->SetValue(iroc,ipad,1);
          continue;
        }
        //exclude edge pads
        if (ipad==0||ipad==npads-1) rocOut->SetValue(iroc,ipad,1); 
        Float_t valTmean=rocData->GetValue(iroc,ipad);
        //exclude values that are exactly 0
        if (valTmean==0) {
          rocOut->SetValue(iroc,ipad,1);
          ++noutliersCE;
        }
        // exclude channels with too large variations
        if (TMath::Abs(valTmean)>fCETmaxLimitAbs) {
          rocOut->SetValue(iroc,ipad,1);
          ++noutliersCE;
        }
      }
    }
  }
  //perform fit
  TMatrixD dummy;
  Float_t chi2A,chi2C;
  fCETmean->GlobalSidesFit(&out,fitFormula,fitResultsA,fitResultsC,dummy,dummy,chi2A,chi2C);
}
//_____________________________________________________________________________________
void AliTPCcalibDButil::ProcessCEgraphs(TVectorD &vecTEntries, TVectorD &vecTMean, TVectorD &vecTRMS, TVectorD &vecTMedian,
                     TVectorD &vecQEntries, TVectorD &vecQMean, TVectorD &vecQRMS, TVectorD &vecQMedian,
                     Float_t &driftTimeA, Float_t &driftTimeC )
{
  //
  // Calculate statistical information from the CE graphs for drift time and charge
  //
  
  //reset arrays
  vecTEntries.ResizeTo(72);
  vecTMean.ResizeTo(72);
  vecTRMS.ResizeTo(72);
  vecTMedian.ResizeTo(72);
  vecQEntries.ResizeTo(72);
  vecQMean.ResizeTo(72);
  vecQRMS.ResizeTo(72);
  vecQMedian.ResizeTo(72);
  vecTEntries.Zero();
  vecTMean.Zero();
  vecTRMS.Zero();
  vecTMedian.Zero();
  vecQEntries.Zero();
  vecQMean.Zero();
  vecQRMS.Zero();
  vecQMedian.Zero();
  driftTimeA=0;
  driftTimeC=0;
  TObjArray *arrT=fCalibDB->GetCErocTtime();
  TObjArray *arrQ=fCalibDB->GetCErocQtime();
  if (arrT){
    for (Int_t isec=0;isec<74;++isec){
      TGraph *gr=(TGraph*)arrT->At(isec);
      if (!gr) continue;
      TVectorD values;
      Int_t npoints = gr->GetN();
      values.ResizeTo(npoints);
      Int_t nused =0;
      //skip first points, theres always some problems with finding the CE position
      for (Int_t ipoint=4; ipoint<npoints; ipoint++){
        if (gr->GetY()[ipoint]>500 && gr->GetY()[ipoint]<1020 ){
          values[nused]=gr->GetY()[ipoint];
          nused++;
        }
      }
      //
      if (isec<72) vecTEntries[isec]= nused;
      if (nused>1){
        if (isec<72){
          vecTMedian[isec] = TMath::Median(nused,values.GetMatrixArray());
          vecTMean[isec]   = TMath::Mean(nused,values.GetMatrixArray());
          vecTRMS[isec]    = TMath::RMS(nused,values.GetMatrixArray());
        } else if (isec==72){
          driftTimeA=TMath::Median(nused,values.GetMatrixArray());
        } else if (isec==73){
          driftTimeC=TMath::Median(nused,values.GetMatrixArray());
        }
      }
    }
  }
  if (arrQ){
    for (Int_t isec=0;isec<arrQ->GetEntriesFast();++isec){
      TGraph *gr=(TGraph*)arrQ->At(isec);
      if (!gr) continue;
      TVectorD values;
      Int_t npoints = gr->GetN();
      values.ResizeTo(npoints);
      Int_t nused =0;
      for (Int_t ipoint=0; ipoint<npoints; ipoint++){
        if (gr->GetY()[ipoint]>500 && gr->GetY()[ipoint]<1000 ){
          values[nused]=gr->GetY()[ipoint];
          nused++;
        }
      }
      //
      vecQEntries[isec]= nused;
      if (nused>1){
        vecQMedian[isec] = TMath::Median(nused,values.GetMatrixArray());
        vecQMean[isec]   = TMath::Mean(nused,values.GetMatrixArray());
        vecQRMS[isec]    = TMath::RMS(nused,values.GetMatrixArray());
      }
    }
  }
}

//_____________________________________________________________________________________
void AliTPCcalibDButil::ProcessNoiseData(TVectorD &vNoiseMean, TVectorD &vNoiseMeanSenRegions,
                      TVectorD &vNoiseRMS, TVectorD &vNoiseRMSSenRegions,
                      Int_t &nonMaskedZero)
{
  //
  // process noise data
  // vNoiseMean/RMS contains the Mean/RMS noise of the complete TPC [0], IROCs only [1],
  //    OROCs small pads [2] and OROCs large pads [3]
  // vNoiseMean/RMSsenRegions constains the same information, but only for the sensitive regions (edge pads, corners, IROC spot)
  // nonMaskedZero contains the number of pads which show zero noise and were not masked. This might indicate an error
  //
  
  //set proper size and reset
  const UInt_t infoSize=4;
  vNoiseMean.ResizeTo(infoSize);
  vNoiseMeanSenRegions.ResizeTo(infoSize);
  vNoiseRMS.ResizeTo(infoSize);
  vNoiseRMSSenRegions.ResizeTo(infoSize);
  vNoiseMean.Zero();
  vNoiseMeanSenRegions.Zero();
  vNoiseRMS.Zero();
  vNoiseRMSSenRegions.Zero();
  nonMaskedZero=0;
  //counters
  TVectorD c(infoSize);
  TVectorD cs(infoSize);
  //tpc parameters
  AliTPCParam par;
  par.Update();
  //retrieve noise and ALTRO data
  if (!fPadNoise) return;
  AliTPCCalROC *rocMasked=0x0;
  //create IROC, OROC1, OROC2 and sensitive region masks
  for (UInt_t isec=0;isec<AliTPCCalPad::kNsec;++isec){
    AliTPCCalROC *noiseROC=fPadNoise->GetCalROC(isec);
    if (fALTROMasked) rocMasked=fALTROMasked->GetCalROC(isec);
    UInt_t nrows=noiseROC->GetNrows();
    for (UInt_t irow=0;irow<nrows;++irow){
      UInt_t npads=noiseROC->GetNPads(irow);
      for (UInt_t ipad=0;ipad<npads;++ipad){
        //don't use masked channels;
        if (rocMasked && rocMasked->GetValue(irow,ipad)) continue;
        Float_t noiseVal=noiseROC->GetValue(irow,ipad);
        //check if noise==0
        if (noiseVal==0) {
          ++nonMaskedZero;
          continue;
        }
        //check for nan
        if ( !(noiseVal<10000000) ){
          printf ("Warning: nan detected in (sec,row,pad - val): %02d,%02d,%03d - %.1f\n",isec,irow,ipad,noiseVal);
          continue;
        }
        Int_t cpad=(Int_t)ipad-(Int_t)npads/2;
        Int_t masksen=1; // sensitive pards are not masked (0)
        if (ipad<2||npads-ipad-1<2) masksen=0; //don't mask edge pads (sensitive)
        if (isec<AliTPCROC::Instance()->GetNInnerSector()){
          //IROCs
          if (irow>19&&irow<46){
            if (TMath::Abs(cpad)<7) masksen=0; //IROC spot
          }
          Int_t type=1;
          vNoiseMean[type]+=noiseVal;
          vNoiseRMS[type]+=noiseVal*noiseVal;
          ++c[type];
          if (!masksen){
            vNoiseMeanSenRegions[type]+=noiseVal;
            vNoiseRMSSenRegions[type]+=noiseVal*noiseVal;
            ++cs[type];
          }
        } else {
          //OROCs
          //define sensive regions
          if ((nrows-irow-1)<3) masksen=0; //last three rows in OROCs are sensitive
          if ( irow>75 ){
            Int_t padEdge=(Int_t)TMath::Min(ipad,npads-ipad);
            if (padEdge<((((Int_t)irow-76)/4+1))*2) masksen=0; //OROC outer corners are sensitive
          }
          if ((Int_t)irow<par.GetNRowUp1()){
            //OROC1
            Int_t type=2;
            vNoiseMean[type]+=noiseVal;
            vNoiseRMS[type]+=noiseVal*noiseVal;
            ++c[type];
            if (!masksen){
              vNoiseMeanSenRegions[type]+=noiseVal;
              vNoiseRMSSenRegions[type]+=noiseVal*noiseVal;
              ++cs[type];
            }
          }else{
            //OROC2
            Int_t type=3;
            vNoiseMean[type]+=noiseVal;
            vNoiseRMS[type]+=noiseVal*noiseVal;
            ++c[type];
            if (!masksen){
              vNoiseMeanSenRegions[type]+=noiseVal;
              vNoiseRMSSenRegions[type]+=noiseVal*noiseVal;
              ++cs[type];
            }
          }
        }
        //whole tpc
        Int_t type=0;
        vNoiseMean[type]+=noiseVal;
        vNoiseRMS[type]+=noiseVal*noiseVal;
        ++c[type];
        if (!masksen){
          vNoiseMeanSenRegions[type]+=noiseVal;
          vNoiseRMSSenRegions[type]+=noiseVal*noiseVal;
          ++cs[type];
        }
      }//end loop pads
    }//end loop rows
  }//end loop sectors (rocs)
  
  //calculate mean and RMS
  const Double_t verySmall=0.0000000001;
  for (UInt_t i=0;i<infoSize;++i){
    Double_t mean=0;
    Double_t rms=0;
    Double_t meanSen=0;
    Double_t rmsSen=0;
    
    if (c[i]>verySmall){
//       printf ("i: %d - m: %.3f, c: %.0f, r: %.3f\n",i,vNoiseMean[i],c[i],vNoiseRMS[i]);
      mean=vNoiseMean[i]/c[i];
      rms=vNoiseRMS[i];
      rms=TMath::Sqrt(TMath::Abs(rms/c[i]-mean*mean));
    }
    vNoiseMean[i]=mean;
    vNoiseRMS[i]=rms;
    
    if (cs[i]>verySmall){
      meanSen=vNoiseMeanSenRegions[i]/cs[i];
      rmsSen=vNoiseRMSSenRegions[i];
      rmsSen=TMath::Sqrt(TMath::Abs(rmsSen/cs[i]-meanSen*meanSen));
    }
    vNoiseMeanSenRegions[i]=meanSen;
    vNoiseRMSSenRegions[i]=rmsSen;
  }
}

//_____________________________________________________________________________________
void AliTPCcalibDButil::ProcessPulser(TVectorD &vMeanTime)
{
  //
  // Process the Pulser information
  // vMeanTime:     pulser mean time position in IROC-A, IROC-C, OROC-A, OROC-C
  //

  const UInt_t infoSize=4;
  //reset counters to error number
  vMeanTime.ResizeTo(infoSize);
  vMeanTime.Zero();
  //counter
  TVectorD c(infoSize);
  //retrieve pulser and ALTRO data
  if (!fPulserTmean) return;
  //
  //get Outliers
  AliTPCCalROC *rocOut=0x0;
  for (UInt_t isec=0;isec<AliTPCCalPad::kNsec;++isec){
    AliTPCCalROC *tmeanROC=fPulserTmean->GetCalROC(isec);
    if (!tmeanROC) continue;
    rocOut=fPulserOutlier->GetCalROC(isec);
    UInt_t nchannels=tmeanROC->GetNchannels();
    for (UInt_t ichannel=0;ichannel<nchannels;++ichannel){
      if (rocOut && rocOut->GetValue(ichannel)) continue;
      Float_t val=tmeanROC->GetValue(ichannel);
      Int_t type=isec/18;
      vMeanTime[type]+=val;
      ++c[type];
    }
  }
  //calculate mean
  for (UInt_t itype=0; itype<infoSize; ++itype){
    if (c[itype]>0) vMeanTime[itype]/=c[itype];
    else vMeanTime[itype]=0;
  }
}
//_____________________________________________________________________________________
void AliTPCcalibDButil::ProcessALTROConfig(Int_t &nMasked)
{
  //
  // Get Values from ALTRO configuration data
  //
  nMasked=-1;
  if (!fALTROMasked) return;
  nMasked=0;
  for (Int_t isec=0;isec<fALTROMasked->kNsec; ++isec){
    AliTPCCalROC *rocMasked=fALTROMasked->GetCalROC(isec);
    for (UInt_t ichannel=0; ichannel<rocMasked->GetNchannels();++ichannel){
      if (rocMasked->GetValue(ichannel)) ++nMasked;
    }
  }
}
//_____________________________________________________________________________________
void AliTPCcalibDButil::ProcessGoofie(TVectorD & vecEntries, TVectorD & vecMedian, TVectorD &vecMean, TVectorD &vecRMS)
{
  //
  // Proces Goofie values, return statistical information of the currently set goofieArray
  // The meaning of the entries are given below
  /*
  1       TPC_ANODE_I_A00_STAT
  2       TPC_DVM_CO2
  3       TPC_DVM_DriftVelocity
  4       TPC_DVM_FCageHV
  5       TPC_DVM_GainFar
  6       TPC_DVM_GainNear
  7       TPC_DVM_N2
  8       TPC_DVM_NumberOfSparks
  9       TPC_DVM_PeakAreaFar
  10      TPC_DVM_PeakAreaNear
  11      TPC_DVM_PeakPosFar
  12      TPC_DVM_PeakPosNear
  13      TPC_DVM_PickupHV
  14      TPC_DVM_Pressure
  15      TPC_DVM_T1_Over_P
  16      TPC_DVM_T2_Over_P
  17      TPC_DVM_T_Over_P
  18      TPC_DVM_TemperatureS1
   */
  if (!fGoofieArray){
    Int_t nsensors=19;
    vecEntries.ResizeTo(nsensors);
    vecMedian.ResizeTo(nsensors);
    vecMean.ResizeTo(nsensors);
    vecRMS.ResizeTo(nsensors);
    vecEntries.Zero();
    vecMedian.Zero();
    vecMean.Zero();
    vecRMS.Zero();
    return;
  }
  Double_t kEpsilon=0.0000000001;
  Double_t kBig=100000000000.;
  Int_t nsensors = fGoofieArray->NumSensors();
  vecEntries.ResizeTo(nsensors);
  vecMedian.ResizeTo(nsensors);
  vecMean.ResizeTo(nsensors);
  vecRMS.ResizeTo(nsensors);
  TVectorF values;
  for (Int_t isensor=0; isensor<fGoofieArray->NumSensors();isensor++){
    AliDCSSensor *gsensor = fGoofieArray->GetSensor(isensor);
    if (gsensor &&  gsensor->GetGraph()){
      Int_t npoints = gsensor->GetGraph()->GetN();
      // filter zeroes
      values.ResizeTo(npoints);
      Int_t nused =0;
      for (Int_t ipoint=0; ipoint<npoints; ipoint++){
        if (TMath::Abs(gsensor->GetGraph()->GetY()[ipoint])>kEpsilon &&
            TMath::Abs(gsensor->GetGraph()->GetY()[ipoint])<kBig ){
              values[nused]=gsensor->GetGraph()->GetY()[ipoint];
              nused++;
            }
      }
      //
      vecEntries[isensor]= nused;
      if (nused>1){
        vecMedian[isensor] = TMath::Median(nused,values.GetMatrixArray());
        vecMean[isensor]   = TMath::Mean(nused,values.GetMatrixArray());
        vecRMS[isensor]    = TMath::RMS(nused,values.GetMatrixArray());
      }
    }
  }
}

//_____________________________________________________________________________________
void AliTPCcalibDButil::UpdatePulserOutlierMap()
{
  //
  // Create a map that contains outliers from the Pulser calibration data.
  // The outliers include masked channels, edge pads and pads with
  //   too large timing and charge variations.
  // nonMaskedZero is the number of outliers in the Pulser calibration data.
  //   those do not contain masked and edge pads
  //
  if (!fPulserTmean||!fPulserQmean) {
    //reset map
    fPulserOutlier->Multiply(0.);
    fNpulserOutliers=-1;
    return;
  }
  AliTPCCalROC *rocMasked=0x0;
  fNpulserOutliers=0;
  
  //Create Outlier Map
  for (UInt_t isec=0;isec<AliTPCCalPad::kNsec;++isec){
    AliTPCCalROC *tmeanROC=fPulserTmean->GetCalROC(isec);
    AliTPCCalROC *qmeanROC=fPulserQmean->GetCalROC(isec);
    AliTPCCalROC *outROC=fPulserOutlier->GetCalROC(isec);
    if (!tmeanROC||!qmeanROC) {
      //reset outliers in this ROC
      outROC->Multiply(0.);
      continue;
    }
    if (fALTROMasked) rocMasked=fALTROMasked->GetCalROC(isec);
//     Double_t dummy=0;
//     Float_t qmedian=qmeanROC->GetLTM(&dummy,.5);
//     Float_t tmedian=tmeanROC->GetLTM(&dummy,.5);
    UInt_t nrows=tmeanROC->GetNrows();
    for (UInt_t irow=0;irow<nrows;++irow){
      UInt_t npads=tmeanROC->GetNPads(irow);
      for (UInt_t ipad=0;ipad<npads;++ipad){
        Int_t outlier=0,masked=0;
        Float_t q=qmeanROC->GetValue(irow,ipad);
        Float_t t=tmeanROC->GetValue(irow,ipad);
        //masked channels are outliers
        if (rocMasked && rocMasked->GetValue(irow,ipad)) masked=1;
        //edge pads are outliers
        if (ipad==0||ipad==npads-1) masked=1;
        //channels with too large charge or timing deviation from the meadian are outliers
//         if (TMath::Abs(q-qmedian)>fPulQmaxLimitAbs || TMath::Abs(t-tmedian)>fPulTmaxLimitAbs) outlier=1;
        if (q<fPulQminLimit && !masked) outlier=1;
        //check for nan
        if ( !(q<10000000) || !(t<10000000)) outlier=1;
        outROC->SetValue(irow,ipad,outlier+masked);
        fNpulserOutliers+=outlier;
      }
    }
  }
}
//_____________________________________________________________________________________
AliTPCCalPad* AliTPCcalibDButil::CreatePadTime0(Int_t model)
{
  //
  // Create pad time0 object from pulser and/or CE data, depending on the selected model
  // Model 0: normalise each readout chamber to its mean, outlier cutted, only Pulser
  // Model 1: normalise IROCs/OROCs of each readout side to its mean, only Pulser
  // 
  //
  
  AliTPCCalPad *padTime0=new AliTPCCalPad("PadTime0",Form("PadTime0-Model_%d",model));
  // decide between different models
  if (model==0||model==1){
    TVectorD vMean;
    if (model==1) ProcessPulser(vMean);
    for (UInt_t isec=0;isec<AliTPCCalPad::kNsec;++isec){
      AliTPCCalROC *rocPulTmean=fPulserTmean->GetCalROC(isec);
      if (!rocPulTmean) continue;
      AliTPCCalROC *rocTime0=padTime0->GetCalROC(isec);
      AliTPCCalROC *rocOut=fPulserOutlier->GetCalROC(isec);
      Float_t mean=rocPulTmean->GetMean(rocOut);
      //treat case where a whole partition is masked
      if (mean==0) mean=rocPulTmean->GetMean();
      if (model==1) {
        Int_t type=isec/18;
        mean=vMean[type];
      }
      UInt_t nrows=rocTime0->GetNrows();
      for (UInt_t irow=0;irow<nrows;++irow){
        UInt_t npads=rocTime0->GetNPads(irow);
        for (UInt_t ipad=0;ipad<npads;++ipad){
          Float_t time=rocPulTmean->GetValue(irow,ipad);
          //in case of an outlier pad use the mean of the altro values.
          //This should be the most precise guess in that case.
          if (rocOut->GetValue(irow,ipad)) {
            time=GetMeanAltro(rocPulTmean,irow,ipad,rocOut);
            if (time==0) time=mean;
          }
          Float_t val=time-mean;
          rocTime0->SetValue(irow,ipad,val);
        }
      }
    }
  }



  return padTime0;
}
//_____________________________________________________________________________________
Float_t AliTPCcalibDButil::GetMeanAltro(const AliTPCCalROC *roc, const Int_t row, const Int_t pad, AliTPCCalROC *rocOut)
{
  if (roc==0) return 0.;
  const Int_t sector=roc->GetSector();
  AliTPCROC *tpcRoc=AliTPCROC::Instance();
  const UInt_t altroRoc=fMapper->GetFEC(sector,row,pad)*8+fMapper->GetChip(sector,row,pad);
  Float_t mean=0;
  Int_t   n=0;
  
  //loop over a small range around the requested pad (+-10 rows/pads)
  for (Int_t irow=row-10;irow<row+10;++irow){
    if (irow<0||irow>(Int_t)tpcRoc->GetNRows(sector)-1) continue;
    for (Int_t ipad=pad-10; ipad<pad+10;++ipad){
      if (ipad<0||ipad>(Int_t)tpcRoc->GetNPads(sector,irow)-1) continue;
      const UInt_t altroCurr=fMapper->GetFEC(sector,irow,ipad)*8+fMapper->GetChip(sector,irow,ipad);
      if (altroRoc!=altroCurr) continue;
      if ( rocOut && rocOut->GetValue(irow,ipad) ) continue;
      Float_t val=roc->GetValue(irow,ipad);
      mean+=val;
      ++n;
    }
  }
  if (n>0) mean/=n;
  return mean;
}
