/*!
 * Title:   FlashHypothesis Creator Class
 * Author:  Wes Ketchum (wketchum@lanl.gov)
 *
 * Description: Class that produces a flash hypothesis for a trajectory.
 * Input:       Trajectory (std::vector<TVector3> objects)
 * Output:      FlashHypotheses
*/

#include "FlashHypothesisCreator.h"

opdet::FlashHypothesisCollection 
opdet::FlashHypothesisCreator::GetFlashHypothesisCollection(recob::Track const& track, 
							    std::vector<float> const& dEdxVector,
							    geo::Geometry const& geom,
							    phot::PhotonVisibilityService const& pvs,
							    const detinfo::LArProperties* larp,
							    opdet::OpDigiProperties const& opdigip,
							    float XOffset)
{
  bool interpolate_dEdx=false;
  if(track.NumberTrajectoryPoints() == dEdxVector.size())
    interpolate_dEdx=true;
  else if(track.NumberTrajectoryPoints() == dEdxVector.size()+1)
    interpolate_dEdx=false;
  else
    throw "ERROR in FlashHypothesisCreator: dEdx vector size not compatible with track size.";

  FlashHypothesisCollection fhc(geom.NOpDets());
  for(size_t pt=1; pt<track.NumberTrajectoryPoints(); pt++){
    if(interpolate_dEdx)
      fhc = fhc + CreateFlashHypothesesFromSegment(track.LocationAtPoint(pt-1),
						   track.LocationAtPoint(pt),
						   0.5*(dEdxVector[pt]+dEdxVector[pt-1]),
						   geom,pvs,larp,opdigip,XOffset);
    else
      fhc = fhc + CreateFlashHypothesesFromSegment(track.LocationAtPoint(pt-1),
						   track.LocationAtPoint(pt),
						   dEdxVector[pt-1],
						   geom,pvs,larp,opdigip,XOffset);
  }
  return fhc;
}

opdet::FlashHypothesisCollection 
opdet::FlashHypothesisCreator::GetFlashHypothesisCollection(sim::MCTrack const& mctrack, 
							    std::vector<float> const& dEdxVector,
							    geo::Geometry const& geom,
							    phot::PhotonVisibilityService const& pvs,
							    const detinfo::LArProperties* larp,
							    opdet::OpDigiProperties const& opdigip,
							    float XOffset)
{
  bool interpolate_dEdx=false;
  if(mctrack.size() == dEdxVector.size())
    interpolate_dEdx=true;
  else if(mctrack.size() == dEdxVector.size()+1)
    interpolate_dEdx=false;
  else
    throw "ERROR in FlashHypothesisCreator: dEdx vector size not compatible with mctrack size.";

  FlashHypothesisCollection fhc(geom.NOpDets());
  for(size_t pt=1; pt<mctrack.size(); pt++){
    if(interpolate_dEdx)
      fhc = fhc + CreateFlashHypothesesFromSegment(mctrack[pt-1].Position().Vect(),
						   mctrack[pt].Position().Vect(),
						   0.5*(dEdxVector[pt]+dEdxVector[pt-1]),
						   geom,pvs,larp,opdigip,XOffset);
    else
      fhc = fhc + CreateFlashHypothesesFromSegment(mctrack[pt-1].Position().Vect(),
						   mctrack[pt].Position().Vect(),
						   dEdxVector[pt-1],
						   geom,pvs,larp,opdigip,XOffset);
  }
  return fhc;
}

opdet::FlashHypothesisCollection 
opdet::FlashHypothesisCreator::GetFlashHypothesisCollection(std::vector<TVector3> const& trajVector, 
							    std::vector<float> const& dEdxVector,
							    geo::Geometry const& geom,
							    phot::PhotonVisibilityService const& pvs,
							    const detinfo::LArProperties* larp,
							    opdet::OpDigiProperties const& opdigip,
							    float XOffset)
{
  bool interpolate_dEdx=false;
  if(trajVector.size() == dEdxVector.size())
    interpolate_dEdx=true;
  else if(trajVector.size() == dEdxVector.size()+1)
    interpolate_dEdx=false;
  else
    throw "ERROR in FlashHypothesisCreator: dEdx vector size not compatible with trajVector size.";

  FlashHypothesisCollection fhc(geom.NOpDets());
  for(size_t pt=1; pt<trajVector.size(); pt++){
    if(interpolate_dEdx)
      fhc = fhc + CreateFlashHypothesesFromSegment(trajVector[pt-1],
						   trajVector[pt],
						   0.5*(dEdxVector[pt]+dEdxVector[pt-1]),
						   geom,pvs,larp,opdigip,XOffset);
    else
      fhc = fhc + CreateFlashHypothesesFromSegment(trajVector[pt-1],
						   trajVector[pt],
						   dEdxVector[pt-1],
						   geom,pvs,larp,opdigip,XOffset);
  }
  return fhc;
}

opdet::FlashHypothesisCollection 
opdet::FlashHypothesisCreator::GetFlashHypothesisCollection(TVector3 const& pt1, TVector3 const& pt2, 
							    float const& dEdx,
							    geo::Geometry const& geom,
							    phot::PhotonVisibilityService const& pvs,
							    const detinfo::LArProperties* larp,
							    opdet::OpDigiProperties const& opdigip,
							    float XOffset)
{
  return CreateFlashHypothesesFromSegment(pt1,pt2,dEdx,geom,pvs,larp,opdigip,XOffset);
}

opdet::FlashHypothesisCollection
opdet::FlashHypothesisCreator::CreateFlashHypothesesFromSegment(TVector3 const& pt1, TVector3 const& pt2, 
							    float const& dEdx,
							    geo::Geometry const& geom,
							    phot::PhotonVisibilityService const& pvs,
							    const detinfo::LArProperties* larp,
							    opdet::OpDigiProperties const& opdigip,
							    float XOffset)
{
  FlashHypothesisCollection fhc(geom.NOpDets());

  FlashHypothesis prompt_hyp = FlashHypothesis(geom.NOpDets());
  
  std::vector<double> xyz_segment(_calc.SegmentMidpoint(pt1,pt2,XOffset));
  
  //get the visibility vector
  const std::vector<float>* PointVisibility = pvs.GetAllVisibilities(&xyz_segment[0]);
  
  //check vector size, as it may be zero if given a y/z outside some range
  if(PointVisibility->size()!=geom.NOpDets()) return fhc;
  
  //klugey ... right now, set a qe_vector that gives constant qe across all opdets
  std::vector<float> qe_vector(geom.NOpDets(),opdigip.QE());
  _calc.FillFlashHypothesis(larp->ScintYield()*larp->ScintYieldRatio(),
			    dEdx,
			    pt1,pt2,
			    qe_vector,
			    *PointVisibility,
			    prompt_hyp);
  
  fhc.SetPromptHypAndPromptFraction(prompt_hyp,larp->ScintYieldRatio());
  return fhc;
}