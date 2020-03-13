#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
// #define EIGEN_USE_MKL_ALL
#include <doctest.h>
#include <array>

#include "Determinants.h"

/**
 * @brief Initializes the determinant member variables for a given set of
 * parameters consistent with HF, i.e. lowest orbitals populated first.
 *
 * @param norbs Number of SPATIAL orbitals.
 * @param nalpha Number of alpha orbitals.
 * @param nbeta Number of beta orbitals.
 * @return Determinant HF determinant.
 */
Determinant HFDeterminantSetup(int norbs, int nalpha, int nbeta) {
  Determinant::EffDetLen = (norbs) / 64 + 1;
  Determinant::norbs = norbs;
  Determinant::n_spinorbs = norbs * 2;
  Determinant::nalpha = nalpha;
  Determinant::nbeta = nbeta;

  Determinant det;

  for (int i = 0; i < Determinant::n_spinorbs; i++) {
    if (i < nalpha) {
      det.setocc(2 * i, true);
    }
    if (i < nbeta) {
      det.setocc(2 * i + 1, true);
    }
  }

  return det;
}

/**
 * @brief Generates a vector of example HF determinants. If you want to add a
 * test case, add it here. The tuples represent norbs (spatial orbs), nalpha,
 * and nbeta respectively.
 *
 * @return std::vector<std::array<int, 3>>
 */
std::vector<std::array<int, 3>> HFDetParams() {
  std::vector<std::array<int, 3>> Dets;

  // Add new configurations here
  Dets.push_back(std::array<int, 3>{8, 6, 6});
  Dets.push_back(std::array<int, 3>{8, 4, 4});
  Dets.push_back(std::array<int, 3>{10, 6, 4});
  Dets.push_back(std::array<int, 3>{11, 5, 4});
  return Dets;
}

TEST_CASE("Determinants: Basics") {
  std::cout << "Testing Determinant Basics" << std::endl;
  int norbs, nalpha, nbeta;

  auto hf_det_params = HFDetParams();
  for (auto det_p : hf_det_params) {
    norbs = det_p[0], nalpha = det_p[1], nbeta = det_p[2];
    auto det = HFDeterminantSetup(norbs, nalpha, nbeta);
    std::cout << det << std::endl;  // JETS: for debugging

    REQUIRE(det.Nalpha() == nalpha);
    REQUIRE(det.Nbeta() == nbeta);
    REQUIRE(det.Noccupied() == nalpha + nbeta);
    REQUIRE(det.hasUnpairedElectrons() == !(nalpha == nbeta));
    REQUIRE(det.numUnpairedElectrons() == nalpha - nbeta);
    REQUIRE(det.parityOfFlipAlphaBeta() == 1.);
    REQUIRE(det.getNalphaBefore(nalpha - 1) == nalpha - 1);
    REQUIRE(det.getNbetaBefore(nbeta - 1) == nbeta - 1);
  }
}

TEST_CASE("Determinants: Parity") {
  std::cout << std::endl << "Testing Determinants: Parity" << std::endl;
  //
  auto det = HFDeterminantSetup(4, 1, 1);
  std::cout << det << std::endl;
  int c = 2, d = 0;
  double parity = 1.;
  det.parity(d, c, parity);
  REQUIRE(parity == -1.);

  c = 2, d = 1;
  parity = 1.0;
  det.parity(d, c, parity);
  REQUIRE(parity == 1.0);

  det.setocc(3, true);
  std::cout << det << std::endl;
  parity = 1.;
  c = 4, d = 0;
  det.parity(d, c, parity);
  REQUIRE(parity == 1.);
}

TEST_CASE("Determinant Helper Functions: GetLadderOps") {
  std::cout << std::endl << "Testing GetLadderOps" << std::endl;

  auto bra = HFDeterminantSetup(4, 1, 1);
  auto ket = bra;
  ket.setocc(1, false);
  ket.setocc(3, true);

  std::cout << bra << std::endl;
  std::cout << ket << std::endl;

  REQUIRE(bra.ExcitationDistance(ket) == 1);

  //
  long ua, ba, ka, ub, bb, kb;
  int cre[2], des[2], ncre = 0, ndes = 0;
  std::cout << cre[0] << " " << cre[1] << std::endl;
  std::cout << des[0] << " " << des[1] << std::endl;
  for (int i = 0; i < Determinant::EffDetLen; i++) {
    // Alpha excitations
    ua = bra.reprA[i] ^ ket.reprA[i];
    ba = ua & bra.reprA[i];  // the cre bits
    ka = ua & ket.reprA[i];  // the des bits
    GetLadderOps(ba, cre, ncre, i, false);
    GetLadderOps(ka, des, ndes, i, false);

    // Beta excitations
    ub = bra.reprB[i] ^ ket.reprB[i];
    bb = ub & bra.reprB[i];  // the cre bits
    kb = ub & ket.reprB[i];  // the des bits
    GetLadderOps(bb, cre, ncre, i, true);
    GetLadderOps(kb, des, ndes, i, true);
  }

  std::cout << cre[0] << " " << cre[1] << std::endl;
  std::cout << des[0] << " " << des[1] << std::endl;

  REQUIRE(ncre == 1);
  REQUIRE(ndes == 1);
}