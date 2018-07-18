/*

   Original Copyright (C) 2007 Lucas K. Wagner

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

*/

#include "Qmc_std.h"
#include "qmc_io.h"
#include "Spinor_Slat_wf_data.h"
#include "Wavefunction_data.h"
#include "Spinor_Slat_wf.h"
#include <algorithm>
#include "MatrixAlgebra.h"
#include <map>
#include <utility>  // for make_pair


/*!
*/
void Spinor_Slat_wf_data::read(vector <string> & words, unsigned int & pos,
    System * sys)
{


  vector <string> strdetwt;
  vector <string> strstates;
  vector <vector <string> > statevec;
  unsigned int startpos=pos;

  pos=startpos;

  while(readsection(words, pos, strstates, "STATES"))
  {
    statevec.push_back(strstates);
  }


  if(readvalue(words, pos=startpos, mo_place, "OPTIMIZE_MO") ) {
    error("Can't optimize MO for spinor slater wf");
  }
  else optimize_mo=0;

  if(haskeyword(words, pos=startpos, "OPTIMIZE_DET")) {
    optimize_det=1;
    if(optimize_mo) error("Can't optimize both mo and det right now");
  }
  else optimize_det=0;

  sort=1;
  if(haskeyword(words, pos=startpos, "NOSORT")) {
    sort=0;
  }

  pos=startpos;
  vector <vector <string> > csfstr;
  vector <string> csfsubstr;

  while(readsection(words, pos, csfsubstr , "CSF")){
    csfstr.push_back(csfsubstr);
  }
  ncsf=csfstr.size();
  if(ncsf){
    CSF.Resize(ncsf);
    int counter=0;
    for(int csf=0;csf<ncsf;csf++){
      if(csfstr[csf].size()<2)
        error(" Wrong number of elements in the CSF number ",csf+1);
      CSF(csf).Resize(csfstr[csf].size());
      for(int j=0;j<CSF(csf).GetDim(0);j++){
        CSF(csf)(j)=atof(csfstr[csf][j].c_str());
        if(j>0)
          counter++;
      }
    }
    ndet=counter;

    if(readsection(words, pos, strdetwt, "DETWT")){
      error("Spinor Slater determinant: CSF is incompatible with DETWT.");
    }
    counter=0;
    detwt.Resize(ndet);
    for(int csf=0;csf<ncsf;csf++)
      for(int j=1;j<CSF(csf).GetDim(0);j++){
        detwt(counter++)=CSF(csf)(0)*CSF(csf)(j);
      }
  }
  else{
    pos=startpos;
    readsection(words, pos, strdetwt, "DETWT");
    ndet=strdetwt.size();
    detwt.Resize(ndet);
    for(int det=0; det < ndet; det++){
      detwt(det)=atof(strdetwt[det].c_str());
    }
    CSF.Resize(ndet);
    ncsf=ndet;
    for(int det=0; det < ndet; det++) { 
      CSF(det).Resize(2);
      CSF(det)(0)=detwt(det).val();
      CSF(det)(1)=1.0;
    }
  }

  if(fabs(CSF(0)(0)) < 1e-10)
    error("Cannot deal with the first CSF having zero weight.");
      

  //no sorting when ndet=1;
  if( ndet==1 && sort)
    sort=0;

  pos=startpos;
  vector <string> mowords;
  if(readsection(words,pos, mowords, "ORBITALS"))  {
    allocate(mowords, sys, molecorb);

    nmo=molecorb->getNmo();
    genmolecorb=molecorb;
    use_complexmo=0;
  }
  else if(readsection(words, pos=0, mowords, "CORBITALS")) {
    allocate(mowords, sys, cmolecorb);
    nmo=cmolecorb->getNmo();
    genmolecorb=cmolecorb;
    use_complexmo=1;
    if(optimize_mo) error("Can't optimize MO's with complex MO's");
    if(optimize_det) 
      error("Don't support optimizing determinants with complex MO's yet");
  }
  else {
    error("Need ORBITALS or CORBITALS section in SLATER wave function");
  }

  nfunc=statevec.size();

  nelectrons=sys->nelectrons(0) + sys->nelectrons(1);
  pos=startpos;
  vector <string> nspinstr;
  if(readsection(words, pos, nspinstr, "NSPIN"))
  {
    error("NSPIN is determined by the system");
    /*
    if(nspinstr.size() != 2)
      error("NSPIN must have 2 elements");
    nelectrons(0)=atoi(nspinstr[0].c_str());
    nelectrons(1)=atoi(nspinstr[1].c_str());
    if(nelectrons(0)+nelectrons(1) != sys->nelectrons(0)+sys->nelectrons(1)) {
      error("NSPIN must specify the same number of electrons as the SYSTEM "
          "in SLATER.");
    }
    */
  }

  pos=startpos;
  unsigned int canonstates=ndet*nelectrons;
  for(int i=0; i< nfunc; i++)
  {
    if( canonstates != statevec[i].size())
    {
      error("in STATES section, expecting to find ", canonstates,
          " states(as calculated from NSPIN), but found ",
          statevec[i].size(), " instead.");
    }
  }




  ndim=3;


  //Input parameters
  occupation.Resize(nfunc, ndet);
  occupation_orig.Resize(nfunc, ndet);



  for(int i=0; i< nfunc; i++) {
    for(int det=0; det < ndet; det++)  {
      occupation(i,det).Resize(nelectrons);
      occupation_orig(i,det).Resize(nelectrons);
    }
  }
  //Calculation helpers
  for(int i=0; i< nfunc; i++) {
    //cout << "i=" << i << endl;
    int counter=0;
    for(int det=0; det<ndet; det++)  {
      //cout << "det=" << det << endl;
      for(int e=0; e<nelectrons; e++) {
        //cout << "e=" << e << endl;
        occupation_orig(i,det)(e)=atoi(statevec[i][counter].c_str())-1;

        counter++;
      }
    }
  }


  //Find what MO's are necessary 
  vector <int> totocctemp;
  for(int f=0; f< nfunc; f++)  {
    for(int det=0; det<ndet; det++) {
      for(int mo=0; mo < nelectrons; mo++) {
        int place=-1;
        int ntot=totocctemp.size();
        for(int i=0; i< ntot; i++) {
          //cout << "i " << i << endl;
          if(occupation_orig(f,det)(mo)==totocctemp[i]) {
            place=i;
            break;
          }
        }
        //cout << "decide " << endl;
        if(place==-1) { //if we didn't find the MO
          //cout << "didn't find it " << endl;
          occupation(f,det)(mo)=totocctemp.size();
          totocctemp.push_back(occupation_orig(f,det)(mo));
        }
        else {
          //cout << "found it" << endl;
          occupation(f,det)(mo)=place;
        }


      }
    }
  }


  totoccupation.Resize(totocctemp.size());
  for(int i=0; i<totoccupation.GetDim(0); i++)
  {
    totoccupation(i) = totocctemp[i];
    //cout << "total occupation for " << s<< " : "
    // << totoccupation(s)(i) << endl;
  }

  excitations.build_excitation_list(occupation,0);
  
  /*
  //Orbital_rotation
  pos=0;
  vector <string> optstring;
  Array1<int> activeSpace;
  if(optimize_mo){
    if(!readsection(words,pos,optstring,"OPTIMIZE_DATA")){
      error("Require section OPTIMIZE_DATA with option optimize_mo");
    }
    //Read and initialize Orbital_rotation
    orbrot=new Orbital_rotation;
    orbrot->read(optstring, detwt.GetDim(0), occupation_orig, occupation, totoccupation);
    nmo=orbrot->getnmo();
  }
  */

  //Decide on the updating scheme:
  
  if(ndet > 1 && nelectrons > 10) {
    use_clark_updates=true;
  }
  else { use_clark_updates=false; } 
  if(haskeyword(words,pos=startpos,"CLARK_UPDATES")) { 
    use_clark_updates=true;
  }
  else if(haskeyword(words,pos=startpos,"SHERMAN_MORRISON_UPDATES")) { 
    use_clark_updates=false;
  }



  //molecorb->buildLists(totoccupation);
  if(genmolecorb) init_mo();


}

//----------------------------------------------------------------------

int Spinor_Slat_wf_data::supports(wf_support_type support) {
  switch(support) {
    case laplacian_update:
      return 1;
    case density:
      return 1;
    case parameter_derivatives:
      return 1;
    default:
      return 0;
  }
}

//----------------------------------------------------------------------

void Spinor_Slat_wf_data::init_mo() {
  int tmpnmo = 0;
  if(optimize_mo){
    nmo=molecorb->getNmo();
  }
  for(int i=0; i< nfunc; i++)
  {
    //cout << "i=" << i << endl;
    int counter=0;
    for(int det=0; det<ndet; det++)
    {
      //cout << "det=" << det << endl;
      //cout << "nelectrons " << nelectrons(s) << endl;
      for(int e=0; e<nelectrons; e++)
      {
        if(tmpnmo < occupation_orig(i,det)(e)+1)
          tmpnmo=occupation_orig(i,det)(e)+1;

        counter++;
      }
    }
  }

  if(tmpnmo > nmo)
    error("determinant contains an orbital higher"
        " than requested NMO's.");
  
  if(optimize_mo){
    error("Can't optimize MOs");
    //nmo=orbrot->getnmo();
  }
  genmolecorb->buildLists(totoccupation);

}

//----------------------------------------------------------------------

void Spinor_Slat_wf_data::generateWavefunction(Wavefunction *& wf)
{
  assert(wf==NULL);
  if(!genmolecorb)
    error("Spinor_Slat_wf_data::Need to allocate molecular orbital before generating any Wavefunction objects");

  if(use_complexmo) {
    //wf=new Cslat_wf;
    //Cslat_wf * slatwf;
    wf=new Spinor_Slat_wf<dcomplex>;
    Spinor_Slat_wf<dcomplex> * slatwf;
    recast(wf,slatwf);
    slatwf->init(this,cmolecorb);
    //slatwf->init(this);
    attachObserver(slatwf);
  }
  else { 
    wf=new Spinor_Slat_wf<doublevar>;
    Spinor_Slat_wf<doublevar> * slatwf;
    recast(wf, slatwf);
    slatwf->init(this,molecorb);
    attachObserver(slatwf);
  }
}


//----------------------------------------------------------------------
int Spinor_Slat_wf_data::showinfo(ostream & os)
{
  if(!genmolecorb)
    error("Spinor_Slat_wf_data::showinfo() : Molecular orbital not allocated");
  os << "Slater Determinant" << endl;

  if(optimize_mo) 
    os << "Optimizing molecular orbital coefficients" << endl;
  if(ndet > 1) 
    os << ndet << " determinants\n";
  else
    os << "1 determinant" << endl;

  if(use_clark_updates) { 
    os << "Using fast updates for multideterminants.  Reference: \n";
    os << "Clark, Morales, McMinis, Kim, and Scuseria. J. Chem. Phys. 135 244105 (2011)\n";
  }

  for(int f=0; f< nfunc; f++) {
    if(nfunc > 1)
      os << "For function " << f << endl;
    for(int det=0; det<ndet; det++) {
      if(ndet > 1) {
        os << "Determinant " << det << ":\n";
        os << "Weight: " << detwt(det).val() << endl;
      }
      os << "State: \n";
      os << "  ";
      for(int e=0; e<nelectrons; e++) {
        os << occupation_orig(f,det)(e)+1 << " ";
        if((e+1)%10 == 0)
          os << endl << "  ";
      }
      os << endl;
    }
  }

  os << "Molecular Orbital object : ";
  genmolecorb->showinfo(os);

  return 1;
}


//----------------------------------------------------------------------

int Spinor_Slat_wf_data::writeinput(string & indent, ostream & os) {

  if(!genmolecorb)
    error("Spinor_Slat_wf_data::writeinput() : Molecular orbital not allocated");

  os << indent << "SPINORSLATER" << endl;

  if(optimize_det)
    os << indent << "OPTIMIZE_DET" << endl;
  if(use_clark_updates)
    os << indent << "CLARK_UPDATES" << endl;
  else 
    os << indent << "SHERMAN_MORRISON_UPDATES" << endl;
  if(!sort)
    os << indent << "NOSORT" << endl;



  Array1 <Array1 <doublevar> > CSF_print(ncsf);
  Array1 <doublevar> detwt_print(ndet);
  Array2 < Array1 <int> > occupation_orig_print(nfunc,ndet);

  if(sort){
    Array1 <doublevar> csf_tmp(ncsf);
    Array1 <int> list;
    for(int csf=0;csf<ncsf;csf++)
      csf_tmp(csf)=CSF(csf)(0);
    sort_abs_values_descending(csf_tmp,csf_tmp,list);


    Array1 < Array1 <int> > det_pos(ncsf);
    int counterr=0;
    for(int csf=0;csf<ncsf;csf++){
      det_pos(csf).Resize(CSF(csf).GetDim(0)-1);
      for(int j=1;j<CSF(csf).GetDim(0);j++){
        det_pos(csf)(j-1)=counterr++;
      }
    }


    //preparing CSF_print
    int counter_new=0;
    for(int csf=0;csf<ncsf;csf++){
      CSF_print(csf).Resize(CSF(list(csf)).GetDim(0));
      for(int j=0;j<CSF(list(csf)).GetDim(0);j++){   
        CSF_print(csf)(j)=CSF(list(csf))(j);
        if(j>0){
          detwt_print(counter_new++)=CSF_print(csf)(0)*CSF_print(csf)(j);
        }
      }
    }

    Array1 <int> detlist(ndet);
    //cout <<"CSF list"<<endl;
    int counter_old=0;
    for(int csf=0;csf<ncsf;csf++){
      //cout << csf<<"  ";
      for(int j=0;j<CSF(list(csf)).GetDim(0)-1;j++){
        //cout <<det_pos(list(csf))(j) <<"  ";
        detlist(counter_old++)=det_pos(list(csf))(j);
      }
      //cout <<endl;
    }


    //preparing occupation_orig_print
    for(int f=0; f< nfunc; f++)
      for(int det=0; det < ndet; det++) {
        occupation_orig_print(f,det).Resize(nelectrons);
        occupation_orig_print(f,det)=occupation_orig(f,detlist(det));
      }
  }
  else{ //no sorting 
    int counter=0;
    for(int csf=0;csf<ncsf;csf++){
      CSF_print(csf).Resize(CSF(csf).GetDim(0));
      for(int j=0;j<CSF(csf).GetDim(0);j++){   
        CSF_print(csf)(j)=CSF(csf)(j);
        if(j>0)
          detwt_print(counter++)=CSF(csf)(0)*CSF(csf)(j);
      }
    }
    for(int f=0; f< nfunc; f++)
      for(int det=0; det < ndet; det++) {
        occupation_orig_print(f,det).Resize(nelectrons);
        occupation_orig_print(f,det)=occupation_orig(f,det);
      }
  }
  // do printout

  for(int csf=0;csf<ncsf;csf++){
    os << indent<<" CSF { ";
    for(int j=0;j<CSF_print(csf).GetDim(0);j++){
      os <<CSF_print(csf)(j)<<"  ";
    } 
    os <<"} "<<endl;
  }
  for(int f=0; f< nfunc; f++){
    os << indent << "STATES { " << endl << indent <<"  ";
    for(int det=0; det < ndet; det++){
      if(ndet>1)
        os <<"#  Determinant "<<det+1<<": weight: "<<detwt_print(det)<<endl<< indent <<"  ";
      for(int e=0; e< nelectrons; e++){
        os << occupation_orig_print(f,det)(e)+1 << " ";
        if((e+1)%20 ==0)
          os << endl << indent << "  ";
      }
      os << endl << indent << "  ";
    }
    os << "}" << endl;
  }
  if(optimize_mo) {
    orbrot->writeinput(indent,os);
  }

  if(use_complexmo) os << indent << "CORBITALS { \n";
  else os << indent << "ORBITALS {\n";
  string indent2=indent+"  ";
  genmolecorb->writeinput(indent2, os);
  os << indent << "}\n";

  return 1;
}

//------------------------------------------------------------------------
void Spinor_Slat_wf_data::getVarParms(Array1 <doublevar> & parms)
{
  //cout <<"start getVarParms"<<endl;
  if(optimize_mo) {
    orbrot->getParms(parms);
  }
  else if(optimize_det) {
    parms.Resize(ncsf-1);
    for(int i=1; i< ncsf; i++) {
      parms(i-1)=CSF(i)(0);
    }
  }
  else {
    parms.Resize(0);
  }
}

//----------------------------------------------------------------------
void Spinor_Slat_wf_data::setVarParms(Array1 <doublevar> & parms)
{
  //cout <<"start setVarParms"<<endl;
  if(optimize_mo) {
    orbrot->setParms(parms);
  }
  else if(optimize_det) {
    for(int csf=1; csf< ncsf; csf++) 
      CSF(csf)(0)=parms(csf-1);
    int counter=0;
    for(int csf=0; csf< ncsf; csf++) {
      for(int j=1;j<CSF(csf).GetDim(0);j++){
        detwt(counter++)=CSF(csf)(0)*CSF(csf)(j);
      }
    }
    assert(counter==ndet);
  }
  else {
    parms.Resize(0);
  }

  int max=wfObserver.size();
  //cout << "slatmax " << max << endl;
  for(int i=0; i< max; i++) {
    wfObserver[i]->notify(all_wf_parms_change, 0);
  }
}
//----------------------------------------------------------------------

void Spinor_Slat_wf_data::linearParms(Array1 <bool> & is_linear) {
  if(optimize_det) { 
    is_linear.Resize(nparms());
    is_linear=true;
  }
  else { 
    is_linear.Resize(nparms());
    is_linear=false;
  }
}

//----------------------------------------------------------------------

void Spinor_Slat_wf_data::renormalize(){
}

//----------------------------------------------------------------------

