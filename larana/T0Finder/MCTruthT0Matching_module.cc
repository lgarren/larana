/////////////////////////////////////////////////////////////////////////////
/// Class:       MCTruthT0Matching
/// Module Type: producer
/// File:        MCTruthT0Matching_module.cc
///
/// Author:         Thomas Karl Warburton
/// E-mail address: k.warburton@sheffield.ac.uk
///
/// Generated at Wed Mar 25 13:54:28 2015 by Thomas Warburton using artmod
/// from cetpkgsupport v1_08_04.
///
/// This module accesses the Monte Carlo Truth information stored in the ART
/// event and matches that with a track. It does this by looping through the
/// tracks in the event and looping through each hit in the track.
/// For each hit it uses the backtracker service to work out the charge which
/// each MCTruth particle contributed to the total charge desposited for the
/// hit.
/// The MCTruth particle which is ultimately assigned to the track is simply
/// the particle which deposited the most charge.
/// It then stores an ART anab::T0 object which has the following variables;
/// 1) Generation time of the MCTruth particle assigned to track, in ns.
/// 2) The trigger type used to assign T0 (in this case 2 for MCTruth)
/// 3) The Geant4 TrackID of the particle (so can access all MCTrtuh info in
///     subsequent modules).
/// 4) The track number of this track in this event.
///
/// The module has been extended to also associate an anab::T0 object with a
/// recob::Shower. It does this following the same algorithm, where
/// recob::Track has been replaced with recob::Shower. 
///
/// The module takes a reconstructed track as input.
/// The module outputs an anab::T0 object
/////////////////////////////////////////////////////////////////////////////

// Framework includes
#include "art/Framework/Core/EDProducer.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "canvas/Persistency/Common/FindManyP.h"
#include "art/Framework/Principal/Handle.h"
#include "art/Framework/Principal/Event.h" 
#include "canvas/Persistency/Common/Ptr.h" 
#include "canvas/Persistency/Common/PtrVector.h" 
#include "art/Framework/Principal/Run.h"
#include "art/Framework/Principal/SubRun.h"
#include "art/Framework/Services/Optional/TFileService.h" 
#include "art/Framework/Services/Optional/TFileDirectory.h"

#include "canvas/Utilities/InputTag.h"
#include "fhiclcpp/ParameterSet.h"
#include "messagefacility/MessageLogger/MessageLogger.h"

#include <memory>
#include <iostream>
#include <map>
#include <iterator>

// LArSoft
#include "larcore/Geometry/Geometry.h"
#include "larcore/Geometry/PlaneGeo.h"
#include "larcore/Geometry/WireGeo.h"
#include "lardata/AnalysisBase/T0.h"
#include "lardata/RecoBase/Hit.h"
#include "lardata/RecoBase/SpacePoint.h"
#include "lardata/RecoBase/Track.h"
#include "lardata/RecoBase/Shower.h"
#include "lardata/Utilities/AssociationUtil.h"
#include "lardata/DetectorInfoServices/LArPropertiesService.h"
#include "lardata/DetectorInfoServices/DetectorPropertiesService.h"
#include "nusimdata/SimulationBase/MCParticle.h"
#include "nusimdata/SimulationBase/MCTruth.h"
#include "larsim/MCCheater/BackTracker.h"
#include "lardata/RawData/ExternalTrigger.h"
#include "larcoreobj/SimpleTypesAndConstants/PhysicalConstants.h"
#include "lardata/AnalysisBase/ParticleID.h"

// ROOT
#include "TTree.h"
#include "TFile.h"

namespace t0 {
  class MCTruthT0Matching;
}

class t0::MCTruthT0Matching : public art::EDProducer {
public:
  explicit MCTruthT0Matching(fhicl::ParameterSet const & p);
  // The destructor generated by the compiler is fine for classes
  // without bare pointers or other resource use.

  // Plugins should not be copied or assigned.
  MCTruthT0Matching(MCTruthT0Matching const &) = delete;
  MCTruthT0Matching(MCTruthT0Matching &&) = delete;
  MCTruthT0Matching & operator = (MCTruthT0Matching const &) = delete; 
 MCTruthT0Matching & operator = (MCTruthT0Matching &&) = delete;

  // Required functions.
  void produce(art::Event & e) override;

  // Selected optional functions.
  void beginJob() override;
  void reconfigure(fhicl::ParameterSet const & p) override;


private:
  
  // Params got from fcl file
  std::string fTrackModuleLabel;
  std::string fShowerModuleLabel;

  // Variable in TFS branches
  TTree* fTree;
  int    TrackID         = 0;
  int    TrueTrackID     = 0;
  int    TrueTriggerType = 0;
  double TrueTrackT0     = 0;  

  int    ShowerID          = 0;
  int    ShowerMatchID     = 0;
  int    ShowerTriggerType = 0;
  double ShowerT0          = 0;
};


t0::MCTruthT0Matching::MCTruthT0Matching(fhicl::ParameterSet const & p)
// :
// Initialize member data here, if know don't want to reconfigure on the fly
{
  // Call appropriate produces<>() functions here.
  produces< std::vector<anab::T0>               >();
  produces< art::Assns<recob::Track , anab::T0> >();
  produces< art::Assns<recob::Shower, anab::T0> > ();
  reconfigure(p);
}

void t0::MCTruthT0Matching::reconfigure(fhicl::ParameterSet const & p)
{
  // Implementation of optional member function here.
  fTrackModuleLabel  = (p.get< std::string > ("TrackModuleLabel" ) );
  fShowerModuleLabel = (p.get< std::string > ("ShowerModuleLabel") );
}

void t0::MCTruthT0Matching::beginJob()
{
  // Implementation of optional member function here.
  art::ServiceHandle<art::TFileService> tfs;
  fTree = tfs->make<TTree>("MCTruthT0Matching","MCTruthT0");
  fTree->Branch("TrueTrackT0",&TrueTrackT0,"TrueTrackT0/D");
  fTree->Branch("TrueTrackID",&TrueTrackID,"TrueTrackID/D");
  //fTree->Branch("ShowerID"   ,&ShowerID   ,"ShowerID/I"   );
  //fTree->Branch("ShowerT0"   ,&ShowerT0   ,"ShowerT0/D"   );
}

void t0::MCTruthT0Matching::produce(art::Event & evt)
{
  if (evt.isRealData()) return;

  // Access art services...
  art::ServiceHandle<geo::Geometry> geom;
  art::ServiceHandle<cheat::BackTracker> bt;

  //TrackList handle
  art::Handle< std::vector<recob::Track> > trackListHandle;
  std::vector<art::Ptr<recob::Track> > tracklist;
  if (evt.getByLabel(fTrackModuleLabel,trackListHandle))
    art::fill_ptr_vector(tracklist, trackListHandle); 
  //if (!trackListHandle.isValid()) trackListHandle.clear();

  //ShowerList handle
  art::Handle< std::vector<recob::Shower> > showerListHandle;
  std::vector<art::Ptr<recob::Shower> > showerlist;
  if (evt.getByLabel(fShowerModuleLabel,showerListHandle))
    art::fill_ptr_vector(showerlist, showerListHandle); 
  //if (!showerListHandle.isValid()) showerListHandle.clear();

  // Create anab::T0 objects and make association with recob::Track
  
  std::unique_ptr< std::vector<anab::T0> > T0col( new std::vector<anab::T0>);
  std::unique_ptr< art::Assns<recob::Track, anab::T0> > Trackassn( new art::Assns<recob::Track, anab::T0>);
  std::unique_ptr< art::Assns<recob::Shower, anab::T0> > Showerassn( new art::Assns<recob::Shower, anab::T0>);

  if (trackListHandle.isValid()){
  //Access tracks and hits
    art::FindManyP<recob::Hit> fmtht(trackListHandle, evt, fTrackModuleLabel);
    
    size_t NTracks = tracklist.size();
    
    // Now to access MCTruth for each track... 
    for(size_t iTrk=0; iTrk < NTracks; ++iTrk) { 
      TrueTrackT0 = 0;
      TrackID     = 0;
      TrueTrackID = 0;
      std::vector< art::Ptr<recob::Hit> > allHits = fmtht.at(iTrk);
      
      std::map<int,double> trkide;
      for(size_t h = 0; h < allHits.size(); ++h){
	art::Ptr<recob::Hit> hit = allHits[h];
	std::vector<sim::IDE> ides;
	std::vector<sim::TrackIDE> TrackIDs = bt->HitToTrackID(hit);
	
	for(size_t e = 0; e < TrackIDs.size(); ++e){
	  trkide[TrackIDs[e].trackID] += TrackIDs[e].energy;
	}
      }
      // Work out which IDE despoited the most charge in the hit if there was more than one.
      double maxe = -1;
      double tote = 0;
      for (std::map<int,double>::iterator ii = trkide.begin(); ii!=trkide.end(); ++ii){
	tote += ii->second;
	if ((ii->second)>maxe){
	  maxe = ii->second;
	  TrackID = ii->first;
	}
      }
      
      // Now have trackID, so get PdG code and T0 etc.
      const simb::MCParticle *particle = bt->TrackIDToParticle(TrackID);
      if (!particle) continue;
      TrueTrackT0 = particle->T();
      TrueTrackID = particle->TrackId();
      TrueTriggerType = 2; // Using MCTruth as trigger, so tigger type is 2.
      
      //std::cout << "Filling T0col with " << TrueTrackT0 << " " << TrueTriggerType << " " << TrueTrackID << " " << (*T0col).size() << std::endl;
      
      T0col->push_back(anab::T0(TrueTrackT0,
				TrueTriggerType,
				TrueTrackID,
				(*T0col).size()
				));
      util::CreateAssn(*this, evt, *T0col, tracklist[iTrk], *Trackassn);    
      fTree -> Fill();  
    } // Loop over tracks   
  }
  
  if (showerListHandle.isValid()){
    art::FindManyP<recob::Hit> fmsht(showerListHandle,evt, fShowerModuleLabel);
    // Now Loop over showers....
    size_t NShowers = showerlist.size();
    for (size_t Shower = 0; Shower < NShowers; ++Shower) {
      ShowerMatchID     = 0;
      ShowerID          = 0;
      ShowerT0          = 0;
      std::vector< art::Ptr<recob::Hit> > allHits = fmsht.at(Shower);
      
      std::map<int,double> showeride;
      for(size_t h = 0; h < allHits.size(); ++h){
	art::Ptr<recob::Hit> hit = allHits[h];
	std::vector<sim::IDE> ides;
	std::vector<sim::TrackIDE> TrackIDs = bt->HitToTrackID(hit);
	
	for(size_t e = 0; e < TrackIDs.size(); ++e){
	  showeride[TrackIDs[e].trackID] += TrackIDs[e].energy;
	}
      }
      // Work out which IDE despoited the most charge in the hit if there was more than one.
      double maxe = -1;
      double tote = 0;
      for (std::map<int,double>::iterator ii = showeride.begin(); ii!=showeride.end(); ++ii){
	tote += ii->second;
	if ((ii->second)>maxe){
	  maxe = ii->second;
	  ShowerID = ii->first;
	}
      }
      // Now have MCParticle trackID corresponding to shower, so get PdG code and T0 etc.
      const simb::MCParticle *particle = bt->TrackIDToParticle(ShowerID);
      if (!particle) continue;
      ShowerT0 = particle->T();
      ShowerID = particle->TrackId();
      ShowerTriggerType = 2; // Using MCTruth as trigger, so tigger type is 2.
      T0col->push_back(anab::T0(ShowerT0,
				ShowerTriggerType,
				ShowerID,
				(*T0col).size()
				));
      util::CreateAssn(*this, evt, *T0col, showerlist[Shower], *Showerassn);    
    }// Loop over showers
  }
  
  evt.put(std::move(T0col));
  evt.put(std::move(Trackassn));
  evt.put(std::move(Showerassn));

} // Produce

DEFINE_ART_MODULE(t0::MCTruthT0Matching)
    
