/*! \file utPHAL_UpdateForceState.cpp */

// ***********************************************************************
//
//                             Peridigm
//                 Copyright (2009) Sandia Corporation
//
// Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
// the U.S. Government retains certain rights in this software.
//
// This library is free software; you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as
// published by the Free Software Foundation; either version 2.1 of the
// License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
// USA
// 
// Questions? 
// David J. Littlewood   djlittl@sandia.gov 
// John A. Mitchell      jamitch@sandia.gov
// Michael L. Parks      mlparks@sandia.gov
// Stewart A. Silling    sasilli@sandia.gov
//
// ***********************************************************************

#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_ALTERNATIVE_INIT_API
#include <boost/test/unit_test.hpp>
#include <Phalanx_DataLayout_MDALayout.hpp>
#include <Epetra_SerialComm.h>
#include "PHAL_FactoryTraits.hpp"
#include "PHAL_Dimension.hpp" // needed for Dummy Data Layout
#include "PHAL_UpdateForceState.hpp"
#include "Peridigm_NeighborhoodData.hpp"
#include "../materials/Peridigm_LinearElasticIsotropicMaterial.hpp"

using namespace boost::unit_test;

void testInitialize()
{
  // set up a parameter list that will be passed to the evaluator constructor
  Teuchos::RCP<Teuchos::ParameterList> p = rcp(new Teuchos::ParameterList);
  int type = FactoryTraits<PHAL::PeridigmTraits>::id_update_force_state;
  p->set<int>("Type", type); 
  p->set<bool>("Verbose", false);

  //! \todo Figure out the purpose of the dummy layout code.
  Teuchos::RCP<PHX::DataLayout> dummy = Teuchos::rcp(new PHX::MDALayout<Dummy>(0));
  p->set< Teuchos::RCP<PHX::DataLayout> >("Dummy Data Layout", dummy);

  // instantiate the evaluator
  UpdateForceState<PHAL::PeridigmTraits::Residual, PHAL::PeridigmTraits> evaluator(*p);

  // assert the evaluator name
  BOOST_CHECK(evaluator.getName() == "UpdateForceState");

  // assert the vector of fields that the evaluator evaluates
  const std::vector<Teuchos::RCP< PHX::FieldTag > > & evaluatedFields = evaluator.evaluatedFields();
  BOOST_REQUIRE(evaluatedFields.size() == 1);
  BOOST_CHECK(evaluatedFields[0]->name() == "UpdateForceState");
  BOOST_CHECK(evaluatedFields[0]->dataLayout() == *dummy);

  // assert the vector of fields that the evaluator is dependent upon
  const std::vector<Teuchos::RCP< PHX::FieldTag > > & dependentFields = evaluator.dependentFields();
  BOOST_CHECK(dependentFields.size() == 0);
}

void testTwoPts()
{
  Epetra_SerialComm comm;

  // set up a hard-coded layout for two points
  int numCells = 2;

  // set up overlap maps, which include ghosted nodes
  // in this case we're on a single processor, so these
  // maps are essentially the identity map
  int numGlobalElements = numCells;
  int numMyElements = numGlobalElements;
  int* myGlobalElements = new int[numMyElements];
  int elementSize = 1;
  for(int i=0; i<numMyElements ; ++i){
	myGlobalElements[i] = i;
  }
  int indexBase = 0;

  // oneDimensionalOverlapMap
  // used for cell volumes and scalar constitutive data
  Epetra_BlockMap oneDimensionalOverlapMap(numGlobalElements, numMyElements, myGlobalElements, elementSize, indexBase, comm); 
  // used for positions, displacements, velocities and vector constitutive data
  elementSize = 3;
  Epetra_BlockMap threeDimensionalOverlapMap(numGlobalElements, numMyElements, myGlobalElements, elementSize, indexBase, comm); 
  delete[] myGlobalElements;

  // create a linear elastic isotropic peridynamic solid  material model
  Teuchos::ParameterList params;
  params.set("Density", 7800.0);
  params.set("Bulk Modulus", 130.0e9);
  params.set("Shear Modulus", 78.0e9);
  PeridigmNS::LinearElasticIsotropicMaterial mat(params);

  // set up the vectors
  Epetra_Vector xOverlap(threeDimensionalOverlapMap);
  Epetra_Vector uOverlap(threeDimensionalOverlapMap);
  Epetra_Vector vOverlap(threeDimensionalOverlapMap);
  Epetra_Vector forceOverlap(threeDimensionalOverlapMap);
  int numScalarConstitutiveVariables = mat.NumScalarConstitutiveVariables();
  BOOST_CHECK(mat.NumScalarConstitutiveVariables() == 3);
  int numVectorConstitutiveVariables = mat.NumVectorConstitutiveVariables();
  BOOST_CHECK(mat.NumVectorConstitutiveVariables() == 1);
  Epetra_MultiVector vectorConstitutiveDataOverlap(threeDimensionalOverlapMap, numVectorConstitutiveVariables);

  // create the material manager
  PeridigmNS::DataManager dataManager;
  dataManager.setScalarMap(Teuchos::rcp(&oneDimensionalOverlapMap, false));
  dataManager.setVector2DMap(Teuchos::null);
  dataManager.setVector3DMap(Teuchos::rcp(&threeDimensionalOverlapMap, false));
  dataManager.allocateData(mat.VariableSpecs());

  // two-point discretization
  double dt = 1.0;
  xOverlap[0] = 0.0; xOverlap[1] = 0.0; xOverlap[2] = 0.0;
  xOverlap[3] = 1.0; xOverlap[4] = 0.0; xOverlap[5] = 0.0;
  uOverlap[0] = 0.0; uOverlap[1] = 0.0; uOverlap[2] = 0.0;
  uOverlap[3] = 0.5; uOverlap[4] = 0.0; uOverlap[5] = 0.0;
  vOverlap[0] = 0.0; vOverlap[1] = 0.0; vOverlap[2] = 0.0;
  vOverlap[3] = 0.5; vOverlap[4] = 0.0; vOverlap[5] = 0.0;
  dataManager.getData(Field_NS::VOLUME, Field_NS::FieldSpec::STEP_NONE)->PutScalar(1.0);
  vectorConstitutiveDataOverlap.PutScalar(0.0);
  Epetra_Vector& weightedVolume = *dataManager.getData(Field_NS::WEIGHTED_VOLUME, Field_NS::FieldSpec::STEP_NONE);
  Epetra_Vector& dilatation = *dataManager.getData(Field_NS::DILATATION, Field_NS::FieldSpec::STEP_NP1);
  weightedVolume[0] = 1.0;
  weightedVolume[1] = 1.0;

  // both points are neighbors of each other
  PeridigmNS::NeighborhoodData neighborhoodData;
  neighborhoodData.SetNumOwned(2);
  neighborhoodData.SetNeighborhoodListSize(4);
  int* const ownedIDs = neighborhoodData.OwnedIDs();
  ownedIDs[0] = 0;
  ownedIDs[1] = 1;
  int* const neighborhoodList = neighborhoodData.NeighborhoodList();
  neighborhoodList[0] = 1;
  neighborhoodList[1] = 1;
  neighborhoodList[2] = 1;
  neighborhoodList[3] = 0;
  int numBonds = 2;
  double* bondData = new double[numBonds];
  bondData[0] = 0.0;
  bondData[1] = 0.0;

  // create a workset with rcps to the relevant data
  PHAL::Workset workset;
  workset.xOverlap = Teuchos::RCP<Epetra_Vector>(&xOverlap, false);
  workset.uOverlap = Teuchos::RCP<Epetra_Vector>(&uOverlap, false);
  workset.vOverlap = Teuchos::RCP<Epetra_Vector>(&vOverlap, false);
  workset.forceOverlap = Teuchos::RCP<Epetra_Vector>(&forceOverlap, false);
  workset.timeStep = Teuchos::RCP<double>(&dt, false);
  workset.neighborhoodData = Teuchos::RCP<PeridigmNS::NeighborhoodData>(&neighborhoodData, false);
  workset.bondData = Teuchos::RCP<double>(bondData, false);
  workset.dataManager = Teuchos::RCP<PeridigmNS::DataManager>(&dataManager, false);
  workset.vectorConstitutiveDataOverlap = Teuchos::RCP<Epetra_MultiVector>(&vectorConstitutiveDataOverlap, false);
  workset.materials = Teuchos::rcp(new std::vector< Teuchos::RCP<const PeridigmNS::Material> >());
  workset.materials->push_back(Teuchos::rcp(&mat, false));
  workset.myPID = comm.MyPID();

  // set up a parameter list that will be passed to the evaluator constructor
  Teuchos::RCP<Teuchos::ParameterList> p = rcp(new Teuchos::ParameterList);
  int type = FactoryTraits<PHAL::PeridigmTraits>::id_update_force_state;
  p->set<int>("Type", type); 
  p->set<bool>("Verbose", false);
  Teuchos::RCP<PHX::DataLayout> dummy = Teuchos::rcp(new PHX::MDALayout<Dummy>(0));
  p->set< Teuchos::RCP<PHX::DataLayout> >("Dummy Data Layout", dummy);

  // instantiate the evaluator
  UpdateForceState<PHAL::PeridigmTraits::Residual, PHAL::PeridigmTraits> evaluator(*p);

  // make a call to the evaluateFields() function in the evaluator
  // this is the workhorse function that calls the material model
  // and updates the values in the constitutive data vectors
  evaluator.evaluateFields(workset);

  // assert the current positions
  double currentPositionX = vectorConstitutiveDataOverlap[0][0];
  BOOST_CHECK_CLOSE(currentPositionX, 0.0, 1.0e-15);
  double currentPositionY = vectorConstitutiveDataOverlap[0][1];
  BOOST_CHECK_CLOSE(currentPositionY, 0.0, 1.0e-15);
  double currentPositionZ = vectorConstitutiveDataOverlap[0][2];
  BOOST_CHECK_CLOSE(currentPositionZ, 0.0, 1.0e-15);
  currentPositionX = vectorConstitutiveDataOverlap[0][3];
  BOOST_CHECK_CLOSE(currentPositionX, 2.0, 1.0e-15);
  currentPositionY = vectorConstitutiveDataOverlap[0][4];
  BOOST_CHECK_CLOSE(currentPositionY, 0.0, 1.0e-15);
  currentPositionZ = vectorConstitutiveDataOverlap[0][5];
  BOOST_CHECK_CLOSE(currentPositionZ, 0.0, 1.0e-15);

  // assert the weighted volumes
  BOOST_CHECK_CLOSE(weightedVolume[0], 1.0, 1.0e-15);
  BOOST_CHECK_CLOSE(weightedVolume[1], 1.0, 1.0e-15);

  // assert the dilatations
  BOOST_CHECK_CLOSE(dilatation[0], 3.0, 1.0e-15);
  BOOST_CHECK_CLOSE(dilatation[1], 3.0, 1.0e-15);

  delete[] bondData;
}

bool init_unit_test_suite()
{
	// Add a suite for each processor in the test
	bool success = true;

	test_suite* proc = BOOST_TEST_SUITE("utPHAL_UpdateForceState");
	proc->add(BOOST_TEST_CASE(&testInitialize));
 	proc->add(BOOST_TEST_CASE(&testTwoPts));
	framework::master_test_suite().add(proc);

	return success;
}

bool init_unit_test()
{
	init_unit_test_suite();
	return true;
}

int main
(int argc, char* argv[])
{
	// Initialize UTF
	return unit_test_main(init_unit_test, argc, argv);
}
