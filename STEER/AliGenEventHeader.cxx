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

/* $Id$ */

// Event header base class for generator. 
// Stores as a minimum the date, run number, event number,
// number of particles produced  
// and the impact parameter.
// 
// Author: andreas.morsch@cern.ch

#include "AliGenEventHeader.h"
ClassImp(AliGenEventHeader)


//_______________________________________________________________________
AliGenEventHeader::AliGenEventHeader():
  fNProduced(-1),
  fVertex(3)
{
  //
  // Constructor
  //
}

//_______________________________________________________________________
AliGenEventHeader::AliGenEventHeader(const char * name):
  TNamed(name, "Event Header"),
  fNProduced(-1),
  fVertex(3)
{
  //
  // Constructor
  //
}

//_______________________________________________________________________
void AliGenEventHeader::SetPrimaryVertex(const TArrayF &o)
{
    //
    // Set the primary vertex for the event
    //
    fVertex[0]=o.At(0);
    fVertex[1]=o.At(1);
    fVertex[2]=o.At(2);
}

//_______________________________________________________________________
void  AliGenEventHeader::PrimaryVertex(TArrayF &o) const
{
    //
    // Return the primary vertex for the event
    //
    o[0] = fVertex.At(0);
    o[1] = fVertex.At(1);
    o[2] = fVertex.At(2);    
}

