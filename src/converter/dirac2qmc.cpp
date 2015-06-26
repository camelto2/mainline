/*
 
Copyright (C) 2007 Lucas K. Wagner

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
 
Author: Cody Melton
Date: 06/25/2015

*/
#include "converter.h"
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <string>
#include <sstream>
#include <vector>
#include <fstream>
#include <cmath>
#include "basis_writer.h"
#include "Pseudo_writer.h"
#include "wf_writer.h"
using namespace std;

template <typename T>
T StringToNumber ( const string &Text ) 
{                              
    stringstream ss(Text);
    T result;
    return ss >> result ? result : 0;
}

template <typename T>
string NumberToString ( T Number )
{
    stringstream ss;
    ss << Number;
    return ss.str();
}

void parse(string &s,vector <string> &parsed_string) {
    stringstream ss(s);
    string buf;
    while (ss >> buf)
	parsed_string.push_back(buf);
}

void skiplines(ifstream &is, int n) {
    string line;
    for (int i = 0; i < n; i++)
	getline(is,line);
}

void read_dirac_mol(string & molfilename,
	            vector <Atom> & atoms,
		    vector <Gaussian_pseudo_writer> & arep,
		    vector <Gaussian_pseudo_writer> & sorep,
		    vector <Gaussian_basis_set> & basis);

void convert_arep_sorep_rrep(vector <Gaussian_pseudo_writer> & arep,
	                     vector <Gaussian_pseudo_writer> & sorep,
			     vector <Gaussian_pseudo_writer> & rrep);

void read_dirac_out(string & outfilename,
	               vector <Atom> & atoms);

void usage(const char * name) {
    cout << "usage: " << name << " <xx.inp> <xx.mol> " << endl;
    exit(1);
}

void test_basis(vector <Gaussian_basis_set> & basis) {
    for (int i = 0; i < basis.size(); i++) {
	cout << "Basis Number: " << i << endl;
	for (int j = 0; j < basis[i].types.size(); j++) {
	    cout << "    Type: " << basis[i].types[j] << endl;
	    for (int k = 0; k < basis[i].exponents[j].size(); k++) 
		cout << "        " << basis[i].exponents[j][k] << endl;
	}
    }
}

void test_atoms(vector <Atom> & atoms) {
    for (int i = 0; i < atoms.size(); i++) {
	cout << "Atom: " << atoms[i].name << endl;
	cout << "      " << atoms[i].charge << endl;
	cout << "      " << "(" <<atoms[i].pos[0] << ", " 
	     << atoms[i].pos[1] << ", " << atoms[i].pos[2] << ") " << endl;
    }
}

void test_pseudo(vector <Gaussian_pseudo_writer> & psp) {
    for (int i = 0; i < psp.size(); i++) {
	cout << "ECP for Atom: " << psp[i].atomnum << endl;
	for (int j = 0; j < psp[i].exponents.size(); j++) {
	    cout << "    Channel: " << j << endl;
	    for (int k = 0; k < psp[i].exponents[j].size(); k++) {
		cout << "        " << psp[i].nvalue[j][k] << " " << psp[i].exponents[j][k] << " " << psp[i].coefficients[j][k] << endl;
	    }
	}
    }
}

//######################################################################

int main(int argc, char ** argv) {

    if (argc != 3) usage(argv[0]);
    
    ifstream test_out; ifstream test_mol; ifstream test_inp;
    string dirac_out=string(argv[1])+"_"+string(argv[2])+".out";
    string dirac_inp=string(argv[1])+".inp";
    string dirac_mol=string(argv[2])+".mol";
    test_inp.open(dirac_inp.c_str());
    test_mol.open(dirac_mol.c_str());
    test_out.open(dirac_out.c_str());
    if (!test_inp) {
        cerr << "Couldn't find " << dirac_inp << endl;
	exit(1);
    }
    else if (!test_mol) {
	cerr << "Couldn't find " << dirac_mol << endl;
	exit(1);
    }
    else if (!test_out) {
	cerr << "Couldn't find " << dirac_out << endl;
	exit(1);
    }
    test_inp.close(); test_inp.clear();
    test_mol.close(); test_mol.clear();
    test_out.close(); test_out.close();

    vector <Atom> atoms;
    vector <Gaussian_pseudo_writer> arep;
    vector <Gaussian_pseudo_writer> sorep;
    vector <Gaussian_pseudo_writer> rrep;
    vector <Gaussian_basis_set> basis;

    read_dirac_mol(dirac_mol,atoms,arep,sorep,basis);
    read_dirac_out(dirac_out,atoms);

    convert_arep_sorep_rrep(arep,sorep,rrep);

    return 0;    
}

//######################################################################

void read_dirac_mol(string & molfilename,
	               vector <Atom> & atoms,
		       vector <Gaussian_pseudo_writer> & arep,
		       vector <Gaussian_pseudo_writer> & sorep,
		       vector <Gaussian_basis_set> & basis) {

    ifstream is(molfilename.c_str());
    if (!is) {
	cout << "Couldn't open " << molfilename << endl;
	exit(1);
    }

    string line;
    vector <string> words;
    int atom_num = 0;
    skiplines(is,3); // 1st 3 lines are comments
    getline(is,line);
    parse(line,words);
    int num_atoms = StringToNumber<int>(words[1]);
    for (int i = 0; i < num_atoms; i++) {
	atoms.push_back(Atom());
	atoms.back().basis = i;
    }
    while(getline(is,line)) {
	parse(line,words);
	// Read basis
	if (line.find("LARGE EXPLICIT") != line.npos) {
	    basis.push_back(Gaussian_basis_set());
	    int num_types=StringToNumber<int>(words[2]); //Number of types
	    //Allocate space for number of types and exponents
	    for (int i = 0; i < num_types; i++) { 
		vector <double> tmp;
		basis.back().exponents.push_back(tmp);
		switch (i) {
		    case 0: basis.back().types.push_back("S");
			break;
		    case 1: basis.back().types.push_back("P");
			break;
	   	    case 2: basis.back().types.push_back("6D");
			break;
		    case 3: basis.back().types.push_back("10F");
			break;
		    case 4: basis.back().types.push_back("15G");
			break;
		    default: cout << "Unsupported basis function" << endl;
			 exit(1);
		}
	    }
	    for (int i = 0; i < basis.back().types.size(); i++) {
		getline(is,line); words.clear(); parse(line,words);
		if (line.find("#") != line.npos) {
		    i -= 1;
		    continue;
		}
		else if (line.find("f") != line.npos) {
    		    for (int j = 0; j < StringToNumber<int>(words[1]); j++) {
			getline(is,line);
			double expt = StringToNumber<double>(line);
			basis.back().exponents[i].push_back(expt);
		    }
		}
	    }
	}
	//Read ECP
	if (line.find("ECP") != line.npos) {
            arep.push_back(Gaussian_pseudo_writer());
            sorep.push_back(Gaussian_pseudo_writer());
	    arep.back().atomnum = atom_num;
	    sorep.back().atomnum = atom_num;
	    int narep, nsorep;
	    narep = StringToNumber<int>(words[2]);
	    nsorep = StringToNumber<int>(words[3]);
	    //Allocate arep & sorep channels
	    for (int i = 0; i < narep; i++) {
                vector <double> tmp;
		vector <int> tmp2;
		arep.back().exponents.push_back(tmp);
		arep.back().coefficients.push_back(tmp);
		arep.back().nvalue.push_back(tmp2);
	    }
	    for (int i = 0; i < nsorep; i++) {
                vector <double> tmp;
		vector <int> tmp2;
		sorep.back().exponents.push_back(tmp);
		sorep.back().coefficients.push_back(tmp);
		sorep.back().nvalue.push_back(tmp2);
	    }
	    //Read AREP
	    for (int i = 0; i < narep; i++) {
                getline(is,line); words.clear(); parse(line,words);
		if (line.find("$") != line.npos || line.find("#") != line.npos) {
		    i -= 1;
		    continue;
		}
		else {
		    int nterms = StringToNumber<int>(words[0]);
		    //allocate number terms for given channel
		    for (int j = 0; j < nterms; j++) {
			getline(is,line); words.clear(); parse(line,words);
			int n = StringToNumber<int>(words[0]);
			double alpha = StringToNumber<double>(words[1]);
			double c = StringToNumber<double>(words[2]);
			arep.back().nvalue[i].push_back(n);
			arep.back().exponents[i].push_back(alpha);
			arep.back().coefficients[i].push_back(c);
		    }
		}
	    }
	    //Read SOREP
	    for (int i = 0; i < nsorep; i++) {
                getline(is,line); words.clear(); parse(line,words);
		if (line.find("$") != line.npos || line.find("#") != line.npos) {
		    i -= 1;
		    continue;
		}
		else {
		    int nterms = StringToNumber<int>(words[0]);
		    //allocate number terms for given channel
		    for (int j = 0; j < nterms; j++) {
			getline(is,line); words.clear(); parse(line,words);
			int n = StringToNumber<int>(words[0]);
			double alpha = StringToNumber<double>(words[1]);
			double c = StringToNumber<double>(words[2]);
			sorep.back().nvalue[i].push_back(n);
			sorep.back().exponents[i].push_back(alpha);
			sorep.back().coefficients[i].push_back(c);
		    }
		}
	    }
	    atom_num+=1;
	}
	words.clear();
    }
    is.close(); is.clear();
}

void convert_arep_sorep_rrep(vector <Gaussian_pseudo_writer> & arep,
	                     vector <Gaussian_pseudo_writer> & sorep,
			     vector <Gaussian_pseudo_writer> & rrep) {

    //Creating RREP for number of atoms. RREP has 2n-2 channels, where
    //n is the number of channels in AREP
    for (int at = 0; at < arep.size(); at++) {
	rrep.push_back(Gaussian_pseudo_writer());
	if (arep[at].nvalue.size() != 1) {
            for (int i = 0; i < (2*arep[at].nvalue.size()-2); i++) {
	        vector <double> tmp;
	        vector <int> tmp2;
	        rrep.back().nvalue.push_back(tmp2);
	        rrep.back().exponents.push_back(tmp);
	        rrep.back().coefficients.push_back(tmp);
	    }
	}
	else {
	     vector <double> tmp;
	     vector <int> tmp2;
	     rrep.back().nvalue.push_back(tmp2);
	     rrep.back().exponents.push_back(tmp);
	     rrep.back().coefficients.push_back(tmp);
	}
    }

    for (int at = 0; at < arep.size(); at++) {
	//Skip the first element in AREP...it is the local channel.
	//Add it last
	rrep[at].atomnum = arep[at].atomnum;
	for (int i = 1; i < arep[at].nvalue.size(); i++) {
	    double l = double(i-1); // because of how arep is stored
	    if (i == 1) {
		for (int j = 0; j < arep[at].coefficients[i].size(); j++) {
		    rrep[at].nvalue[i-1].push_back(arep[at].nvalue[i][j]);
		    rrep[at].exponents[i-1].push_back(arep[at].exponents[i][j]);
		    rrep[at].coefficients[i-1].push_back(arep[at].coefficients[i][j]);
		}
	    }
	    else {
		for (int j = 0; j < arep[at].coefficients[i].size(); j++) {
		    // l j=l+1/2
		    rrep[at].nvalue[i-1].push_back(arep[at].nvalue[i][j]);
		    rrep[at].exponents[i-1].push_back(arep[at].exponents[i][j]);
		    rrep[at].coefficients[i-1].push_back(arep[at].coefficients[i][j]+0.5*l*sorep[at].coefficients[i-2][j]);
		    // l j=l-1/2
		    rrep[at].nvalue[i].push_back(arep[at].nvalue[i][j]);
		    rrep[at].exponents[i].push_back(arep[at].exponents[i][j]);
		    rrep[at].coefficients[i].push_back(arep[at].coefficients[i][j]-0.5*(l+1.0)*sorep[at].coefficients[i-2][j]);
		}
	    }
	}
	//add local channel to end of RREP
	for (int i = 0; i < arep[at].nvalue[0].size(); i++) {
	    rrep[at].nvalue.back().push_back(arep[at].nvalue[0][i]);
	    rrep[at].exponents.back().push_back(arep[at].exponents[0][i]);
	    rrep[at].coefficients.back().push_back(arep[at].coefficients[0][i]);
	}
    }
}

void read_dirac_out(string & outfilename,
	               vector <Atom> & atoms) {

    ifstream is(outfilename.c_str());
    if (!is) {
	cout << "Couldn't open " << outfilename << endl;
	exit(1);
    }

    string line;
    vector <string> words;
    int num_atoms;

    while(getline(is,line)) {
        parse(line,words);
	//Atom names and charges
        if (line.find("Atoms and basis") != line.npos) {
	    skiplines(is,3);
	    words.clear(); getline(is,line); parse(line,words);
	    num_atoms = StringToNumber<int>(words[4]);
            skiplines(is,3);
	    for (int i = 0; i < num_atoms; i++) {
		getline(is,line); words.clear(); parse(line,words);
		atoms[i].name = words[0];
		atoms[i].charge = StringToNumber<double>(words[2]);
		getline(is,line);
	    }
	}
	//Atom Positions
	if (line.find("Cartesian Coord") != line.npos) {
	    skiplines(is,5);
	    for (int i = 0; i < num_atoms; i++) {
                getline(is,line); words.clear(); parse(line,words);
		atoms[i].pos[0] = StringToNumber<double>(words[3]);
                getline(is,line); words.clear(); parse(line,words);
		atoms[i].pos[1] = StringToNumber<double>(words[2]);
                getline(is,line); words.clear(); parse(line,words);
		atoms[i].pos[2] = StringToNumber<double>(words[2]);
                skiplines(is,1);
	    }
	}
    }

    is.close(); is.clear();
}
