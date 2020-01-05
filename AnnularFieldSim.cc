#include "TVector3.h"
#include "AnnularFieldSim.h"
#include "TH3F.h"

#define ALMOST_ZERO 0.00001

AnnularFieldSim::AnnularFieldSim(float rin, float rout, float dz,
				 int r, int roi_r0, int roi_r1,
				 int phi, int roi_phi0, int roi_phi1,
				 int z, int roi_z0, int roi_z1,
				 float vdr){
  //check well-ordering:
  if (roi_r0 >=r || roi_r1>r || roi_r0>=roi_r1){
    assert(1==2);
  }
  if (roi_phi0 >=phi || roi_phi1>phi || roi_phi0>=roi_phi1){
    printf("phi roi is out of range or spans the wrap-around.  Please spare me that math.\n");
    assert(1==2);
  }
  if (roi_z0 >=z || roi_z1>z || roi_z0>=roi_z1){
    assert(1==2);
  }

  printf("AnnularFieldSim::AnnularFieldSim with (%dx%dx%d) grid\n",r,phi,z);

    Escale=1; Bscale=1;
  vdrift=vdr;
  nr=r;nphi=phi;nz=z;
  rmin=rin; rmax=rout;
  zmin=0;zmax=dz;

  rmin_roi=roi_r0; rmax_roi=roi_r1; nr_roi=rmax_roi-rmin_roi;
  phimin_roi=roi_phi0; phimax_roi=roi_phi1; nphi_roi=phimax_roi-phimin_roi;
  zmin_roi=roi_z0; zmax_roi=roi_z1; nz_roi=zmax_roi-zmin_roi;
 
  
  //define the size of the volume:
  dim.SetXYZ(1,0,0);
  dim.SetPerp(rmax-rmin);
  dim.SetPhi(0);
  phispan=2*TMath::Pi();
  dim.SetZ(dz);

  //define the step:
  //note that you have to set a non-zero value to start or perp won't update.
  step.SetXYZ(1,0,0);
  step.SetPerp(dim.Perp()/nr);
  step.SetPhi(phispan/nphi);
  step.SetZ(dim.Z()/nz);
  // printf("stepsize:  r=%f,phi=%f, wanted %f,%f\n",step.Perp(),step.Phi(),dr/r,dphi/phi);
  


  

  
  //create a field grid for the roi with the specified dimensions
  Efield=new MultiArray<TVector3>(nr_roi,nphi_roi,nz_roi);
  for (int i=0;i<Efield->Length();i++)
    Efield->GetFlat(i)->SetXYZ(0,0,0);

  //and a grid to compute the field in each cell given charge in each other cell.
  Epartial=new MultiArray<TVector3>(nr_roi,nphi_roi,nz_roi,nr,nphi,nz);
  for (int i=0;i<Epartial->Length();i++)
    Epartial->GetFlat(i)->SetXYZ(0,0,0);

  //and to hold the external fieldmap over the region of interest
  Eexternal=new MultiArray<TVector3>(nr_roi,nphi_roi,nz_roi);
  for (int i=0;i<Eexternal->Length();i++)
    Eexternal->GetFlat(i)->SetXYZ(0,0,0);

  //ditto the external magnetic fieldmap
  Bfield=new MultiArray<TVector3>(nr_roi,nphi_roi,nz_roi);
  for (int i=0;i<Bfield->Length();i++)
    Bfield->GetFlat(i)->SetXYZ(0,0,0);

  //and a grid of charges in the entire source volume, regardless of roi.
  //TODO:  this could be changed to be a TH3F...
  //space charge in coulomb units.
  q=new MultiArray<float>(nr,nphi,nz);
  for (int i=0;i<q->Length();i++)
    *(q->GetFlat(i))=0;



  //define the external fields:
  setFlatFields(1.4/*tesla*/,200/*V/cm*/);
  return;
}


AnnularFieldSim::AnnularFieldSim(float rin, float rout, float dz,int r, int phi, int z, float vdr)
  :
  AnnularFieldSim( rin,  rout,  dz,
		   r,0, r,
		   phi, 0, phi,
		   z, 0, z,
		   vdr)
{
  return;
}

TVector3 AnnularFieldSim::calc_unit_field(TVector3 at, TVector3 from){
  //note this is the field due to a fixed point charge in free space.
  //if doing cylindrical calcs with different boundary conditions, this needs to change.

  //this could check roi bounds before returning, if things start acting funny.
  
  const float k=8.987*1e13;//=1/(4*pi*eps0) in N*cm^2/C^2 in a vacuum. N*cm^2/C units, so that we supply space charge in coulomb units.
  TVector3 delr=at-from;
  TVector3 field=delr; //to set the direction.
  if (delr.Mag()<ALMOST_ZERO*ALMOST_ZERO){ //note that this has blurred units -- it should scale with all three dimensions of stepsize.  For lots of phi bins, especially, this might start to misbehave.
    //do nothing.  the vector is already zero, which will be our approximation.
    //field.SetMag(0);//no contribution if we're in the same cell. -- but root warns if trying to resize something of magnitude zero.
  } else{
    field.SetMag(k*1/(delr*delr));//scalar product on the bottom.
  }
  //printf("calc_unit_field at (%2.2f,%2.2f,%2.2f) from  (%2.2f,%2.2f,%2.2f).  Mag=%2.4fe-9\n",at.x(),at.Y(),at.Z(),from.X(),from.Y(),from.Z(),field.Mag()*1e9);
  
  return field;
}
int AnnularFieldSim::FilterPhiIndex(int phi){
  //shifts phi up or down by nphi (=2pi in phi index space), and complains if this doesn't put it in range.
  int p=phi;
  if (p>=nphi){
    p-=nphi;
  }
  if (p<0){
    p+=nphi;
  }
  if (p>=nphi || p<0){
    printf("AnnularFieldSim::FilterPhiIndex asked to filter %d, which is more than nphi=%d out of bounds.  Check what called this.\n",phi,nphi);
  }
  return p;
}
  
AnnularFieldSim::BoundsCase AnnularFieldSim::GetRindexAndCheckBounds(float pos, int *r){
  float r0f=(pos-rmin)/step.Perp(); //the position in r, in units of step, starting from the low edge of the 0th bin.
  int r0=floor(r0f);
  *r=r0;

    int r0lowered_slightly=floor(r0f-ALMOST_ZERO);
  int r0raised_slightly=floor(r0f+ALMOST_ZERO); 
  if (r0lowered_slightly>=rmax_roi || r0raised_slightly<rmin_roi){
    return OutOfBounds;
  }

    //now if we are out of bounds, it is because we are on an edge, within range of ALMOST_ZERO of being in fair territory.
  if (r0>=rmax_roi){
    return OnHighEdge;
  }
  if (r0<rmin_roi){
    return OnLowEdge;
  }
  //if we're still here, we're in bounds.
  return InBounds;

}
AnnularFieldSim::BoundsCase AnnularFieldSim::GetPhiIndexAndCheckBounds(float pos, int *phi){
  float p0f=(pos)/step.Phi(); //the position in phi, in units of step, starting from the low edge of the 0th bin.
  int p0=FilterPhiIndex(floor(p0f));
  *phi=p0;

  int p0lowered_slightly=FilterPhiIndex(floor(p0f-ALMOST_ZERO));
  int p0raised_slightly=FilterPhiIndex(floor(p0f+ALMOST_ZERO));
  //annoying detail:  if we are at index 0, we might go above pmax by going down.
  // and if we are at nphi-1, we might go below pmin by going up.
  // if we are not at p0=0 or nphi-1, the slight wiggles won't move us.
  // if we are at p0=0, we are not at or above the max, and lowering slightly won't change that,
  // and is we are at p0=nphi-1, we are not below the min, and raising slightly won't change that
  if ((p0lowered_slightly>=phimax_roi && p0!=0)  || (p0raised_slightly<phimin_roi && p0!=(nphi-1))){
    return OutOfBounds;
  }
 //now if we are out of bounds, it is because we are on an edge, within range of ALMOST_ZERO of being in fair territory.
  if (p0>=phimax_roi){
    return OnHighEdge;
  }
  if (p0<phimin_roi){
    return OnLowEdge;
  }
  //if we're still here, we're in bounds.
  return InBounds;

}
AnnularFieldSim::BoundsCase AnnularFieldSim::GetZindexAndCheckBounds(float pos, int *z){
  float z0f=(pos-zmin)/step.Z(); //the position in z, in units of step, starting from the low edge of the 0th bin.
  int z0=floor(z0f);
  *z=z0;

  int z0lowered_slightly=floor(z0f-ALMOST_ZERO);
  int z0raised_slightly=floor(z0f+ALMOST_ZERO); 

  if (z0lowered_slightly>=zmax_roi || z0raised_slightly<zmin_roi){
    return OutOfBounds;
  }
  //now if we are out of bounds, it is because we are on an edge, within range of ALMOST_ZERO of being in fair territory.
  if (z0>=zmax_roi){
    return OnHighEdge;
  }
  if (z0<zmin_roi){
    return OnLowEdge;
  }
  //if we're still here, we're in bounds.
  return InBounds;
}
  


 


    

TVector3 AnnularFieldSim::fieldIntegral(float zdest,TVector3 start){
  //integrates E dz, from the starting point to the selected z position.  The path is assumed to be along z for each step, with adjustments to x and y accumulated after each step.
  // printf("AnnularFieldSim::fieldIntegral(x=%f,y=%f, z=%f) to z=%f\n",start.X(),start.Y(),start.Z(),zdest);

  int r, phi;
  bool rOkay=  (GetRindexAndCheckBounds(start.Perp(), &r) == InBounds);
  bool phiOkay=  (GetPhiIndexAndCheckBounds(start.Phi(), &phi) == InBounds);

  if (!rOkay || !phiOkay){
    printf("AnnularFieldSim::fieldIntegral asked to integrate along (r=%f,phi=%f), index=(%d,%d), which is out of bounds.  returning starting position.\n",start.Perp(),start.Phi(),r,phi);
    return start;
  }
  
  int dir=(start.Z()>zdest?-1:1);//+1 if going to larger z, -1 if going to smaller;  if they're the same, the sense doesn't matter.

  int zi, zf;
  double startz,endz;
  BoundsCase startBound,endBound;

  //make sure 'zi' is always the smaller of the two numbers, for handling the partial-steps.
  if (dir>0){
    startBound=GetZindexAndCheckBounds(start.Z(),&zi); //highest cell with lower bound less than lower bound of integral
    endBound=GetZindexAndCheckBounds(zdest,&zf); //highest cell with lower bound less than upper lower bound of integral
    startz=start.Z();
    endz=zdest;
  } else{
    endBound=GetZindexAndCheckBounds(start.Z(),&zf); //highest cell with lower bound less than lower bound of integral
    startBound=GetZindexAndCheckBounds(zdest,&zi); //highest cell with lower bound less than upper lower bound of integral
    startz=zdest;
    endz=start.Z();
  }
  bool startOkay=(startBound==InBounds); 
  bool endOkay=(endBound==InBounds || endBound==OnHighEdge); //if we just barely touch out-of-bounds on the high end, we can skip that piece of the integral
  
  if (!startOkay || !endOkay){
    printf("AnnularFieldSim::fieldIntegral asked to integrate from z=%f to %f, index=%d to %d), which is out of bounds.  returning starting position.\n",startz,endz,zi,zf);
    return start;
  }
 
  TVector3 fieldInt(0,0,0);
  // printf("AnnularFieldSim::fieldIntegral requesting (%d,%d,%d)-(%d,%d,%d) (inclusive) cells\n",r,phi,zi,r,phi,zf-1);
  for(int i=zi;i<zf;i++){ //count the whole cell of the lower end, and skip the whole cell of the high end.
    TVector3 tf=Efield->Get(r-rmin_roi,phi-phimin_roi,i-zmin_roi);
      //printf("fieldAt (%d,%d,%d)=(%f,%f,%f) step=%f\n",r,phi,i,tf.X(),tf.Y(),tf.Z(),step.Z());
      fieldInt+=tf*step.Z();
  }
  
  //since bins contain their lower bound, but not their upper, I can safely remove the unused portion of the lower cell:
    fieldInt-=Efield->Get(r-rmin_roi,phi-phimin_roi,zi-zmin_roi)*(startz-zi*step.Z());//remove the part of the low end cell we didn't travel through

//but only need to add the used portion of the upper cell if we go past the edge of it meaningfully:
    if (endz/step.Z()-zf>ALMOST_ZERO){
      //printf("endz/step.Z()=%f, zf=%f\n",endz/step.Z(),zf*1.0);
      //if our final step is actually in the next step.
      fieldInt+=Efield->Get(r-rmin_roi,phi-phimin_roi,zf-zmin_roi)*(endz-zf*step.Z());//add the part of the high end cell we did travel through
    }

    
  return dir*fieldInt;
}

TVector3 AnnularFieldSim::GetCellCenter(int r, int phi, int z){
  //returns the midpoint of the cell (halfway between each edge, not weighted center)
  
  TVector3 c(1,0,0);
  c.SetPerp((r+0.5)*step.Perp()+rmin);
  c.SetPhi((phi+0.5)*step.Phi());
  c.SetZ((z+0.5)*step.Z());

  return c;
}

TVector3 AnnularFieldSim::GetWeightedCellCenter(int r, int phi, int z){
  //returns the weighted center of the cell by volume.
  //todo:  this is vaguely hefty, and might be worth storing the result of, if speed is needed
  TVector3 c(1,0,0);

  float rin=r*step.Perp()+rmin;
  float rout=rin+step.Perp();

  float rMid=(4*TMath::Sin(step.Phi()/2)*(pow(rout,3)-pow(rin,3))
	      /(3*step.Phi()*(pow(rout,2)-pow(rin,2))));
  c.SetPerp(rMid);
  c.SetPhi((phi+0.5)*step.Phi());
  c.SetZ((z+0.5)*step.Z());

  return c;
}



TVector3 AnnularFieldSim::interpolatedFieldIntegral(float zdest,TVector3 start){
  printf("AnnularFieldSim::interpolatedFieldIntegral(x=%f,y=%f, z=%f)\n",start.X(),start.Y(),start.Z());


  float r0=(start.Perp()-rmin)/step.Perp()-0.5; //the position in r, in units of step, starting from the center of the 0th bin.
  int r0i=floor(r0); //the integer portion of the position. -- what center is below our position?
  float r0d=r0-r0i;//the decimal portion of the position. -- how far past center are we?
  int ri[4];//the r position of the four cell centers to consider
  ri[0]=ri[1]=r0i;
  ri[2]=ri[3]=r0i+1;
  float rw[4];//the weight of that cell, linear in distance from the center of it
  rw[0]=rw[1]=1-r0d; //1 when we're on it, 0 when we're at the other one.
  rw[2]=rw[3]=r0d; //1 when we're on it, 0 when we're at the other one

  bool skip[]={false,false,false,false};
  if (ri[0]<rmin_roi){
    skip[0]=true; //don't bother handling 0 and 1 in the coordinates.
    skip[1]=true;
    rw[2]=rw[3]=1; //and weight like we're dead-center on the outer cells.
  }
  if (ri[2]>=rmax_roi){
    skip[2]=true; //don't bother handling 2 and 3 in the coordinates.
    skip[3]=true;
    rw[0]=rw[1]=1; //and weight like we're dead-center on the inner cells.
  }
  
  //now repeat that structure for phi:
  float p0=(start.Phi())/step.Phi()-0.5; //the position in phi, in units of step, starting from the center of the 0th bin.
  int p0i=floor(p0); //the integer portion of the position. -- what center is below our position?
  float p0d=p0-p0i;//the decimal portion of the position. -- how far past center are we?
  int pi[4];//the phi position of the four cell centers to consider
  pi[0]=pi[2]=FilterPhiIndex(p0i);
  pi[1]=pi[3]=FilterPhiIndex(p0i+1);
  float pw[4];//the weight of that cell, linear in distance from the center of it
  pw[0]=pw[2]=1-p0d; //1 when we're on it, 0 when we're at the other one.
  pw[1]=pw[3]=p0d; //1 when we're on it, 0 when we're at the other one

  if (pi[0]<phimin_roi){
    skip[0]=true; //don't bother handling 0 and 1 in the coordinates.
    skip[2]=true;
    pw[1]=pw[3]=1; //and weight like we're dead-center on the outer cells.
  }
  if (pi[1]>=phimax_roi){
    skip[1]=true; //don't bother handling 2 and 3 in the coordinates.
    skip[3]=true;
    pw[0]=pw[2]=1; //and weight like we're dead-center on the inner cells.
  }
  

   //printf("interpolating fieldInt at  r=%f,phi=%f\n",r0,phi0);

  int dir=(start.Z()>zdest?-1:1);//+1 if going to larger z, -1 if going to smaller;  if they're the same, the sense doesn't matter.

  int zi, zf;
  double startz,endz;
  BoundsCase startBound,endBound;

  //make sure 'zi' is always the smaller of the two numbers, for handling the partial-steps.
  if (dir>0){
    startBound=GetZindexAndCheckBounds(start.Z(),&zi); //highest cell with lower bound less than lower bound of integral
    endBound=GetZindexAndCheckBounds(zdest,&zf); //highest cell with lower bound less than upper lower bound of integral
    startz=start.Z();
    endz=zdest;
  } else{
    endBound=GetZindexAndCheckBounds(start.Z(),&zf); //highest cell with lower bound less than lower bound of integral
    startBound=GetZindexAndCheckBounds(zdest,&zi); //highest cell with lower bound less than upper lower bound of integral
    startz=zdest;
    endz=start.Z();
  }
  bool startOkay=(startBound==InBounds); //maybe todo: add handling for being just below the low edge.
  bool endOkay=(endBound==InBounds || endBound==OnHighEdge); //if we just barely touch out-of-bounds on the high end, we can skip that piece of the integral
  
  if (!startOkay || !endOkay){
    printf("AnnularFieldSim::InterpolatedFieldIntegral asked to integrate from z=%f to %f, index=%d to %d), which is out of bounds.  returning starting position.\n",startz,endz,zi,zf);
    return start;
  }




  


  TVector3 fieldInt, partialInt;//where we'll store integrals as we generate them.
  
  for (int i=0;i<4;i++){
    if (skip[i]) continue; //we invalidated this one for some reason.
    partialInt.SetXYZ(0,0,0);
    printf("looking for element r=%d,phi=%d\n",ri[i],pi[i]);
    for(int j=zi;j<zf;j++){ //count the whole cell of the lower end, and skip the whole cell of the high end.
      
      partialInt+=Efield->Get(ri[i]-rmin_roi,pi[i]-phimin_roi,j-zmin_roi)*step.Z();
    }
  
    partialInt-=Efield->Get(ri[i]-rmin_roi,pi[i]-phimin_roi,zi-zmin_roi)*(startz-zi*step.Z());//remove the part of the low end cell we didn't travel through
    if (endz/step.Z()-zf>ALMOST_ZERO){
    partialInt+=Efield->Get(ri[i]-rmin_roi,pi[i]-phimin_roi,zf-zmin_roi)*(endz-zf*step.Z());//add the part of the high end cell we did travel through
    }
    fieldInt+=rw[i]*pw[i]*partialInt;
  }
    
  return dir*fieldInt;
}

void AnnularFieldSim::load_spacecharge(TH3F *hist, float zoffset, float scalefactor=1){
  //load spacecharge densities from a histogram, where scalefactor translates into local units
  //noting that the histogram limits may differ from the simulation size, and have different granularity
  //hist is assumed/required to be x=phi, y=r, z=z
  //z offset 'drifts' the charge by that distance toward z=0.

  //Get dimensions of input
  float hrmin=hist->GetYaxis()->GetXmin();
  float hrmax=hist->GetYaxis()->GetXmax();
  float hphimin=hist->GetXaxis()->GetXmin();
  float hphimax=hist->GetXaxis()->GetXmax();
  float hzmin=hist->GetZaxis()->GetXmin();
  float hzmax=hist->GetZaxis()->GetXmax();
  
  //Get number of bins in each dimension
  int hrn=hist->GetNbinsY();
  int hphin=hist->GetNbinsX();
  int hzn=hist->GetNbinsZ();

  //do some computation of steps:
  float hrstep=(hrmax-hrmin)/hrn;
  float hphistep=(hphimax-hphimin)/hphin;
  float hzstep=(hzmax-hzmin)/hzn;

  //clear the previous spacecharge dist:
    for (int i=0;i<q->Length();i++)
    *(q->GetFlat(i))=0;

  
  //loop over every bin and add that to the internal model:
  //note that bin 0 is the underflow, so we need the +1 internally

  //the minimum r we need is localr=0, hence:
  //hr=localr*dr+rmin
  //localr*dr+rmin-hrmin=hrstep*(i+0.5)
  //i=(localr*dr+rmin-hrmin)/hrstep

  for (int i=(rmin-hrmin)/hrstep;i<hrn;i++){
    float hr=hrmin+hrstep*(i+0.5);//bin center
    int localr=(hr-rmin)/step.Perp();
    if (localr<0){
      printf("Loading from histogram has r out of range! r=%f < rmin=%f\n",hr,rmin);      
      continue;
    }
    if (localr>=nr){
      printf("Loading from histogram has r out of range! r=%f > rmax=%f\n",hr,rmax);
      i=hrn; //no reason to keep looking at larger r.
      continue;
    }
    for (int j=0;j<hphin;j++){
      float hphi=hphimin+hphistep*(j+0.5); //bin center
      int localphi=hphi/step.Phi();
      if (localphi>=nphi){//handle wrap-around:
	localphi-=nphi;
      }
      if (localphi<0){//handle wrap-around:
	localphi+=nphi;
      }
      //todo:  should add ability to take in a phi- slice only
      for (int k=(zmin-(hzmin-zoffset))/hzstep;k<hzn;k++){
	float hz=hzmin-zoffset+hzstep*(k+0.5);//bin center
	int localz=hz/step.Z();
	if (localz<0){
	  printf("Loading from histogram has z out of range! z=%f < zmin=%f\n",hz,zmin);
	  continue;
	}
	if (localz>=nz){
	  printf("Loading from histogram has z out of range! z=%f > zmax=%f\n",hz,zmax);
	  k=hzn;//no reason to keep looking at larger z.
	  continue;
	}
	//can be simplified:  float vol=hzstep*(hphistep*(hr+hrstep)*(hr+hrstep) - hphistep*hr*hr);
	float vol=hzstep*hphistep*(2*hr+hrstep)*hrstep;
	float qbin=scalefactor*vol*hist->GetBinContent(hist->GetBin(j+1,i+1,k+1));
	float qold=q->Get(localr,localphi,localz);
	//printf("loading Q=%f from hist(%d,%d,%d) into cell (%d,%d,%d), qold=%f\n",qbin,i,j,k,localr,localphi,localz,qold);
	q->Set(localr,localphi,localz,qbin+qold);
      }
    }
  }
  

  return;
}

void AnnularFieldSim::populate_fieldmap(){
  //sum the E field at every point in the region of interest
  // remember that Efield uses relative indices
  //printf("in pop_fieldmap, n=(%d,%d,%d)\n",nr,ny,nz);
  for (int ir=rmin_roi;ir<rmax_roi;ir++){
    for (int iphi=phimin_roi;iphi<phimax_roi;iphi++){
      for (int iz=zmin_roi;iz<zmax_roi;iz++){
	//*f[ix][iy][iz]=sum_field_at(nr,ny,nz,partial_,q_,ix,iy,iz);
	TVector3 localF=sum_field_at(ir,iphi,iz);
	Efield->Set(ir-rmin_roi,iphi-phimin_roi,iz-zmin_roi,localF);
	//if (localF.Mag()>1e-9)
	//printf("fieldmap@ (%d,%d,%d) mag=%f\n",ir,iphi,iz,localF.Mag());
      }
    }
  }
  return;
}

void  AnnularFieldSim::populate_lookup(){
  //with 'f' being the position the field is being measured at, and 'o' being the position of the charge generating the field.
  //remember the 'f' part of Epartial uses relative indices.
  //  TVector3 (*f)[fx][fy][fz][ox][oy][oz]=field_;
  //printf("populating lookup for (%dx%dx%d)x(%dx%dx%d) grid\n",fx,fy,fz,ox,oy,oz);
  TVector3 at(1,0,0);
  TVector3 from(1,0,0);

  for (int ifr=rmin_roi;ifr<rmax_roi;ifr++){
    for (int ifphi=phimin_roi;ifphi<phimax_roi;ifphi++){
      for (int ifz=zmin_roi;ifz<zmax_roi;ifz++){
	at=GetCellCenter(ifr, ifphi, ifz);
	for (int ior=0;ior<nr;ior++){
	  for (int iophi=0;iophi<nphi;iophi++){
	    for (int ioz=0;ioz<nz;ioz++){
	      from=GetCellCenter(ior, iophi, ioz);

	      //*f[ifx][ify][ifz][iox][ioy][ioz]=cacl_unit_field(at,from);
	      //printf("calc_unit_field...\n");
	      Epartial->Set(ifr-rmin_roi,ifphi-phimin_roi,ifz-zmin_roi,ior,iophi,ioz,calc_unit_field(at,from));
	    }
	  }
	}
      }
    }
  }
  return;

}

void AnnularFieldSim::setFlatFields(float B, float E){
  //these only cover the roi, but since we address them flat, we don't need to know that here.
  printf("AnnularFieldSim::setFlatFields(B=%f,E=%f)\n",B,E);
  printf("lengths:  Eext=%d, Bfie=%d\n",Eexternal->Length(),Bfield->Length());
  for (int i=0;i<Eexternal->Length();i++)
    Eexternal->GetFlat(i)->SetXYZ(0,0,E);
  for (int i=0;i<Bfield->Length();i++)
    Bfield->GetFlat(i)->SetXYZ(0,0,B);
  return;
}

TVector3 AnnularFieldSim::sum_field_at(int r,int phi, int z){
 //sum the E field over all nr by ny by nz cells of sources, at the specific position r,phi,z.
  //note the specific position in Epartial is in relative coordinates.
  //printf("AnnularFieldSim::sum_field_at(r=%d,phi=%d, z=%d)\n",r,phi,z);
  TVector3 sum(0,0,0);
  for (int ir=0;ir<nr;ir++){
    for (int iphi=0;iphi<nphi;iphi++){
      for (int iz=0;iz<nz;iz++){
	//sum+=*partial[x][phi][z][ix][iphi][iz] * *q[ix][iphi][iz];
	if (r==ir && phi==iphi && z==iz) continue;
	sum+=Epartial->Get(r-rmin_roi,phi-phimin_roi,z-zmin_roi,ir,iphi,iz)*q->Get(ir,iphi,iz);
      }
    }
  }
  sum+=Escale*Eexternal->Get(r-rmin_roi,phi-phimin_roi,z-zmin_roi);
  //printf("summed field at (%d,%d,%d)=(%f,%f,%f)\n",x,y,z,sum.X(),sum.Y(),sum.Z());
  return sum;
}

TVector3 AnnularFieldSim::swimToInSteps(float zdest,TVector3 start,int steps=1, bool interpolate=false){
  //short-circuit if we're out of range:
  
  double zdist=zdest-start.Z();
  double zstep=zdist/steps;
  
  TVector3 ret=start;
  for (int i=0;i<steps;i++){
    int rt,pt,zt; //just placeholders
    BoundsCase zBound=GetZindexAndCheckBounds(ret.Z(),&zt);
    if (GetRindexAndCheckBounds(ret.Perp(),&rt)!=InBounds
	|| GetPhiIndexAndCheckBounds(ret.Phi(),&pt)!=InBounds
	|| (zBound!=InBounds && zBound!=OnHighEdge)){
      printf("AnnularFieldSim::swimToInSteps at step %d, asked to swim particle from (%f,%f,%f) which is outside the ROI.  Returning original position.\n",i,ret.X(),ret.Y(),ret.Z());
      return ret;
    }
    ret=swimTo(start.Z()+zstep*(i+1),ret,false);
  }
  
  return ret;
}

TVector3 AnnularFieldSim::swimTo(float zdest,TVector3 start, bool interpolate=false){

 //using second order langevin expansion from http://skipper.physics.sunysb.edu/~prakhar/tpc/Papers/ALICE-INT-2010-016.pdf
  //TVector3 (*field)[nr][ny][nz]=field_;
  int rt,pt,zt; //just placeholders
  BoundsCase zBound=GetZindexAndCheckBounds(start.Z(),&zt);
  if (GetRindexAndCheckBounds(start.Perp(),&rt)!=InBounds
      || GetPhiIndexAndCheckBounds(start.Phi(),&pt)!=InBounds
      || (zBound!=InBounds && zBound!=OnHighEdge)){
    printf("AnnularFieldSim::swimTo asked to swim particle from (%f,%f,%f) which is outside the ROI.  Returning original position.\n",start.X(),start.Y(),start.Z());
    return start;
  }
  
  //set the direction of the external fields.
  //todo: get this from a field map
  TVector3 B=Bfield->Get(0,0,0)*Bscale;//static field in tesla T=Vs/m^2
  
  //int x=start.X()/step.X();
  //int y=start.Y()/step.Y();
  //int zi=start.Z()/step.Z();
  double zdist=zdest-start.Z();

  //short-circuit if there's no travel length:
  if (TMath::Abs(zdist)<ALMOST_ZERO*step.Z()){
    printf("Asked to swim particle from (%f,%f,%f) to z=%f, which is a distance of %fcm.  Returning original position.\n",start.X(),start.Y(),start.Z(), zdest,zdist);
    return start;
  }

  TVector3 fieldInt;
  if (interpolate){
    fieldInt=interpolatedFieldIntegral(zdest,start);
  }else{
    fieldInt=fieldIntegral(zdest,start);
  }
  //float fieldz=field_[in3(x,y,0,fx,fy,fz)].Z()+E.Z();// *field[x][y][zi].Z();
  double fieldz=fieldInt.Z()/zdist;// average field over the path.
  
  double mu=vdrift/fieldz;//cm^2/(V*s);
  double omegatau=1*mu*B.Z();//mu*Q_e*B, units cm^2/m^2
  //originally the above was q*mu*B, but 'q' is really about flipping the direction of time.  if we do this, we negate both vdrift and q, so in the end we have no charge dependence -- we 'see' the charge by noting that we're asking to drift against the overall field.
  omegatau=omegatau*1e-4;//1m/100cm * 1m/100cm to get proper unitless.
  //printf("omegatau=%f\n",omegatau);
  double c0=1/(1+omegatau*omegatau);
  double c1=c0*omegatau;
  double c2=c1*omegatau;

  //really this should be the integral of the ratio, not the ratio of the integrals.
  //and should be integrals over the B field, btu for now that's fixed and constant across the region, so not necessary
  //there's no reason to do this as r phi.  This is an equivalent result, since I handle everything in vectors.
  double deltaX=c0*fieldInt.X()/fieldz+c1*fieldInt.Y()/fieldz-c1*B.Y()/B.Z()*zdist+c2*B.X()/B.Z()*zdist;
  double deltaY=c0*fieldInt.Y()/fieldz-c1*fieldInt.X()/fieldz+c1*B.X()/B.Z()*zdist+c2*B.Y()/B.Z()*zdist;
  double deltaZ=0; //not correct, but small?  different E leads to different drift velocity.  see linked paper.  fix eventually.

  //printf("swimTo: (%2.4f,%2.4f,%2.4f) to z=%2.4f\n",start.X(),start.Y(), start.Z(),zdest);
  //printf("swimTo: fieldInt=(%2.4f,%2.4f,%2.4f)\n",fieldInt.X(),fieldInt.Y(),fieldInt.Z());
  
  TVector3 dest(start.X()+deltaX,start.Y()+deltaY,zdest+deltaZ);
  
  return dest;
  
}


float AnnularFieldSim::RosseggerEterm(int m, int n, TVector3 at, TVector3 from){
  //from eqn 5.68 in Rossegger thesis, p 111.
  //cylindrical cavity from r=a to r=b, z=0 to z=L
  /*
  double L;
  double betaMN; // comes from solution to 5.12.
  double Esubz;
  int deltaM0;
  double eps0;
  double phi1;
  double phi;
  double r1;
  double r;
  double z1;
  double z;
  double dz;
  double dr;
  double dphi;
  
  
  */
  
  return 0;
}