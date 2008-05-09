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

/* $Id: AliACORDEReconstructor.cxx 20956 2007-09-26 14:22:18Z mrodrigu $ */
//////////////////////////////////////////////////////////////////////////////
//                                                                          //
//  Class for ACORDE reconstruction                                         //
//////////////////////////////////////////////////////////////////////////////

#include "AliRawReader.h"

#include "AliACORDEReconstructor.h"
#include "AliACORDERawStream.h"
#include "AliESDEvent.h"
#include "AliACORDEdigit.h"

ClassImp(AliACORDEReconstructor)

AliACORDEReconstructor:: AliACORDEReconstructor():
  AliReconstructor(),
  fESDACORDE(0x0),
  fCalibData(0x0)
{
  // Default constructor  
  // Get calibration data

  fCalibData = GetCalibData(); 
}

//FALTA IMPLEMENTAR_______________________________________________________________________
AliACORDECalibData *AliACORDEReconstructor::GetCalibData() const
{
  // TO BE IMPLEMENTED !!
  return 0x0;
}
//_____________________________________________________________________________
AliACORDEReconstructor& AliACORDEReconstructor::operator = 
  (const AliACORDEReconstructor& /*reconstructor*/)
{
// assignment operator

  Fatal("operator =", "assignment operator not implemented");
  return *this;
}

//_____________________________________________________________________________
AliACORDEReconstructor::~AliACORDEReconstructor()
{
// destructor
//NECESITAS esta clase
  delete fESDACORDE; 
}

//_____________________________________________________________________________
void AliACORDEReconstructor::Init()
{
// initializer
//NECESITAS esta clase
    fESDACORDE  = new AliESDACORDE;
}

void AliACORDEReconstructor::ConvertDigits(AliRawReader* rawReader, TTree* digitsTree) const
{

  if (!digitsTree) {
    AliError("No digits tree!");
    return;
  }

  TClonesArray* digitsArray = new TClonesArray("AliACORDEdigit");
  digitsTree->Branch("ACORDEdigit", &digitsArray);

  rawReader->Reset();
  AliACORDERawStream rawStream(rawReader);
  if (rawStream.Next()) {
    for(Int_t iChannel = 0; iChannel < 60; iChannel++) {
      Int_t  index = iChannel / 30;
      Int_t  bit   = iChannel % 30;
      if (rawStream.GetWord(index) & (1 << bit))
        new ((*digitsArray)[digitsArray->GetEntriesFast()]) AliACORDEdigit(iChannel+1,0);
    }
  }

  digitsTree->Fill();
			
}

void AliACORDEReconstructor::FillESD(TTree* digitsTree, TTree* /*clustersTree*/,AliESDEvent* esd) const
{

  // fills ESD with ACORDE Digits

  if (!digitsTree)
    {
      AliError("No digits tree!");
      return;
    }

  TClonesArray* digitsArray = NULL;
  TBranch* digitBranch = digitsTree->GetBranch("ACORDEdigit");
  if (!digitBranch) {
    AliError("No ACORDE digits branch found!");
    return;
  }
  digitBranch->SetAddress(&digitsArray);

  digitsTree->GetEvent(0);

  Bool_t AcoADCSingle[60],AcoADCMulti[60];
  for(Int_t i = 0; i < 60; i++) { AcoADCSingle[i] = AcoADCMulti[i] = kFALSE; }

  Int_t nDigits = digitsArray->GetEntriesFast();
    
  for (Int_t d=0; d<nDigits; d++) {    
    AliACORDEdigit* digit = (AliACORDEdigit*)digitsArray->At(d);
    Int_t module = digit->GetModule();

    AcoADCSingle[module-1] = kTRUE;
    AcoADCMulti[module-1] = kTRUE;
  }
	
  fESDACORDE->SetACORDESingleMuon(AcoADCSingle);
  fESDACORDE->SetACORDEMultiMuon(AcoADCMulti);

  if (esd)
    {
      AliDebug(1, Form("Writing ACORDE data to ESD Tree"));
      esd->SetACORDEData(fESDACORDE);
    }	
}


