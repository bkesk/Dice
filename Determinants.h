#ifndef Determinants_HEADER_H
#define Determinants_HEADER_H

#include "global.h"
#include <iostream>
#include <vector>
#include <boost/serialization/serialization.hpp>

class oneInt;
class twoInt;

using namespace std;
inline int BitCount (long& u)
{
  if (u==0) return 0;
  unsigned int u2=u>>32, u1=u;
  
  u1 = u1
    - ((u1 >> 1) & 033333333333)
    - ((u1 >> 2) & 011111111111);
  
  
  u2 = u2
    - ((u2 >> 1) & 033333333333)
    - ((u2 >> 2) & 011111111111);
  
  return (((u1 + (u1 >> 3))
	   & 030707070707) % 63) +
    (((u2 + (u2 >> 3))
      & 030707070707) % 63);
}


//This is used to store just the alpha or the beta sub string of the entire determinant
class HalfDet {
 private:
  friend class boost::serialization::access;
  template<class Archive> 
  void serialize(Archive & ar, const unsigned int version) {
    for (int i=0; i<DetLen/2; i++)
      ar & repr[i];
  }
 public:
  long repr[DetLen/2];
  static int norbs;
  HalfDet() {
    for (int i=0; i<DetLen/2; i++)
      repr[i] = 0;
  }

  //the comparison between determinants is performed
  bool operator<(const HalfDet& d) const {
    for (int i=DetLen/2-1; i>=0 ; i--) {
      if (repr[i] < d.repr[i]) return true;
      else if (repr[i] > d.repr[i]) return false;
    }
    return false;
  }

  bool operator==(const HalfDet& d) const {
    for (int i=DetLen/2-1; i>=0 ; i--) 
      if (repr[i] != d.repr[i]) return false;
    return true;    
  }

  //set the occupation of the ith orbital
  void setocc(int i, bool occ) {
    //assert(i< norbs);
    long Integer = i/64, bit = i%64, one=1;
    if (occ)
      repr[Integer] |= one << bit;
    else
      repr[Integer] &= ~(one<<bit);
  }

  //get the occupation of the ith orbital
  bool getocc(int i) const {
    //assert(i< norbs);
    long Integer = i/64, bit = i%64, reprBit = repr[Integer];
    if(( reprBit>>bit & 1) == 0)
      return false;
    else
      return true;
  }
  
  int getClosed(vector<int>& closed){
    int cindex = 0;
    for (int i=0; i<32*DetLen; i++) {
      if (getocc(i)) {closed[cindex] = i; cindex++;}
    }
    return cindex;
  }

  friend ostream& operator<<(ostream& os, const HalfDet& d) {
    char det[norbs/2];
    d.getRepArray(det);
    for (int i=0; i<norbs/2; i++)
      os<<(int)(det[i])<<" ";
    return os;
  }

  void getRepArray(char* repArray) const {
    for (int i=0; i<norbs/2; i++) {
      if (getocc(i)) repArray[i] = 1;
      else repArray[i] = 0;
    }
  }

};

class Determinant {

 private:
  friend class boost::serialization::access;
  template<class Archive> 
  void serialize(Archive & ar, const unsigned int version) {
    for (int i=0; i<DetLen; i++)
      ar & repr[i];
  }

 public:
  // 0th position of 0th long is the first position
  // 63rd position of the last long is the last position
  long repr[DetLen];
  static int norbs;
  static int EffDetLen;

  Determinant() {
    for (int i=0; i<DetLen; i++)
      repr[i] = 0;
  }


  //Is the excitation between *this and d less than equal to 2.
  bool connected(const Determinant& d) const {
    int ndiff = 0; long u;
    for (int i=0; i<EffDetLen; i++) {
      u = repr[i] ^ d.repr[i];
      ndiff += BitCount(u);
      if (ndiff > 4) return false;
    }
    return true;
  }

  //Get the number of electrons that need to be excited to get determinant d from *this determinant
  //e.g. single excitation will return 1
  int ExcitationDistance(const Determinant& d) const {
    int ndiff = 0; long u;
    for (int i=0; i<EffDetLen; i++) {
      u = repr[i] ^ d.repr[i];
      ndiff += BitCount(u);
    }
    return ndiff/2;
  }

  //Get HalfDet with just the alpha string
  HalfDet getAlpha() const {
    HalfDet d;
    for (int i=0; i<EffDetLen; i++)
      for (int j=0; j<32; j++) {
	d.setocc(i*32+j, getocc(i*64+j*2));
      }
    return d;
  }


  //get HalfDet with just the beta string
  HalfDet getBeta() const {
    HalfDet d;
    for (int i=0; i<EffDetLen; i++)
      for (int j=0; j<32; j++)
	d.setocc(i*32+j, getocc(i*64+j*2+1));
    return d;
  }


  //the comparison between determinants is performed
  bool operator<(const Determinant& d) const {
    for (int i=EffDetLen-1; i>=0 ; i--) {
      if (repr[i] < d.repr[i]) return true;
      else if (repr[i] > d.repr[i]) return false;
    }
    return false;
  }

  //check if the determinants are equal
  bool operator==(const Determinant& d) const {
    for (int i=EffDetLen-1; i>=0 ; i--) 
      if (repr[i] != d.repr[i]) return false;
    return true;    
  }

  //set the occupation of the ith orbital
  void setocc(int i, bool occ) {
    //assert(i< norbs);
    long Integer = i/64, bit = i%64, one=1;
    if (occ)
      repr[Integer] |= one << bit;
    else
      repr[Integer] &= ~(one<<bit);
  }


  //get the occupation of the ith orbital
  bool getocc(int i) const {
    //asser(i<norbs);
    long Integer = i/64, bit = i%64, reprBit = repr[Integer];
    if(( reprBit>>bit & 1) == 0)
      return false;
    else
      return true;
  }

  //the represenation where each char represents an orbital
  //have to be very careful that the repArray is properly allocated with enough space
  //before it is passed to this function
  void getRepArray(char* repArray) const {
    for (int i=0; i<norbs; i++) {
      if (getocc(i)) repArray[i] = 1;
      else repArray[i] = 0;
    }
  }

  //Prints the determinant
  friend ostream& operator<<(ostream& os, const Determinant& d) {
    char det[norbs];
    d.getRepArray(det);
    for (int i=0; i<norbs; i++)
      os<<(int)(det[i])<<" ";
    return os;
  }

  //returns integer array containing the closed and open orbital indices
  int getOpenClosed(unsigned short* open, unsigned short* closed) const {
    int oindex=0,cindex=0;
    for (int i=0; i<norbs; i++) {
      if (getocc(i)) {closed[cindex] = i; cindex++;}
      else {open[oindex] = i; oindex++;}
    }
    return cindex;
  }

  //returns integer array containing the closed and open orbital indices
  void getOpenClosed(vector<int>& open, vector<int>& closed) const {
    int oindex=0,cindex=0;
    for (int i=0; i<norbs; i++) {
      if (getocc(i)) {closed.at(cindex) = i; cindex++;}
      else {open.at(oindex) = i; oindex++;}
    }
  }

  //returns integer array containing the closed and open orbital indices
  int getOpenClosed(int* open, int* closed) const {
    int oindex=0,cindex=0;
    for (int i=0; i<norbs; i++) {
      if (getocc(i)) {closed[cindex] = i; cindex++;}
      else {open[oindex] = i; oindex++;}
    }
    return cindex;
  }

};


//how many occupied orbitals before i
double parity(char* d, int& sizeA, int& i);
//energy of the determinant given as char array
double Energy(char* ket, int& sizeA, oneInt& I1, twoInt& I2, double& coreE);
//energy of the determinant given as a integer vector
double Energy(vector<int>& occ, int& sizeA, oneInt& I1, twoInt& I2, double& coreE);

//energy of the new determinant formed by making a double excitation
//INPUT
// 1. closed - the list of occupied orbitals in the input determinant
// 2. nclosed - the number of occupied orbitals
// 3. I1, I2, coreE - one electron, two electron integrals and core energy
// 4. i, A, j, B - orbital indides indicating a double excitation
// 5. Energyd - energy of the determinant represented by the closed array string 
double EnergyAfterExcitation(vector<int>& closed, int& nclosed, oneInt& I1, twoInt& I2, double& coreE,
			     int i, int A, int j, int B, double Energyd);


//energy of the new determinant formed by making a single excitation
//INPUT
// 1. closed - the list of occupied orbitals in the input determinant
// 2. nclosed - the number of occupied orbitals
// 3. I1, I2, coreE - one electron, two electron integrals and core energy
// 4. i, A, - orbital indides indicating a single excitation
// 5. Energyd - energy of the determinant represented by the closed array string 
double EnergyAfterExcitation(vector<int>& closed, int& nclosed, oneInt& I1, twoInt& I2, double& coreE,
			     int i, int A, double Energyd);

//All these functions calcualte the <bra|H|ket> elements
//1. this takes both bra and ket as char arrays
double Hij(char* bra, char* ket, int& sizeA, oneInt& I1, twoInt& I2, double& coreE);
//2. This takes actual determinants
double Hij(Determinant& bra, Determinant& ket, int& sizeA, oneInt& I1, twoInt& I2, double& coreE);
//3. this only takes the ket and the single excitation that gives a bra state
double Hij_1Excite(int i, int a, oneInt& I1, twoInt& I2, char* ket, int& sizeA);
//4. this only takes the ket and the double excitation that gives a bra state
double Hij_2Excite(int i, int j, int a, int b, twoInt& I2, char* ket, int& sizeA);



#endif
