/*
 * @file    main.cpp
 * @author  Neet Patel
 * @date    06/12/2016
 * @brief   Simple MIPS assembler
 *
 * @section DESCRIPTION
 * This program encodes a subset of MIPS assembly language instructions
 * and outputs to the standard out. Program was compiled with various
 * checks that utilized loops, but they were left out for readability
 * upon finalization of the program.
*/

#include <iostream>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <string>
#include <unordered_map>
#include <list>
#include <boost/tokenizer.hpp>

/**********************
     TYPEDEFS
***********************/
enum fields {opfn, rs, rt, rd, shmt, lim};

typedef std::pair<std::string, uint16_t> IAP;
typedef boost::char_separator<char> CharSep;
typedef boost::tokenizer<CharSep> Tokenizer;
typedef std::unordered_map<std::string, uint8_t> OMAP;
typedef std::unordered_map<std::string, uint8_t> RMAP;
typedef std::unordered_map<std::string, uint16_t> SMAP;
typedef std::unordered_map<std::string, uint16_t> DMAP;
typedef std::list<IAP> INLIST;
typedef std::list<uint32_t> OBLIST;

/**********************
  FUNCTION PROTOTYPES
***********************/

void initOpTable (OMAP& opmap);
void initRegTable (RMAP& regmap);
void readText (std::ifstream& ifile, std::string& istring, INLIST& ilist, SMAP& smap, size_t& addrs, size_t& gp);
void readData (std::ifstream& ifile, std::string& istring, std::string& tok, INLIST& ilist, SMAP& smap, size_t& addrs,
               Tokenizer& tokstring, Tokenizer::iterator& tokiter);
void assembleR (std::string fArray[], const RMAP& regmap, const uint8_t& funct, uint32_t& objcode);
void assembleI (std::string fArray[], const RMAP& regmap, const SMAP& smap, bool label,
                const size_t& gp, uint32_t& objcode, const INLIST::iterator& li);
void assembleData (const Tokenizer& tokstring, Tokenizer::iterator& tokiter, OBLIST& oblist, uint32_t& objcode);

/*********************
        MAIN
**********************/
int main (int argc, char* argv[]) {
  if (argc < 2) {
    std::cout << "***No file entered! Please eneter a file name to open***\n";
    return EXIT_FAILURE;
  }
  size_t addrs{0};
  size_t gp{0};
  std::string istring;
  INLIST ilist;
  OMAP opmap;
  RMAP regmap;
  SMAP smap;
  OBLIST oblist;

  initOpTable(opmap); // Opcode/Funct table. R ins. have 0 as their MSB while I ins. have 1.
  initRegTable(regmap); // Register table

  std::ifstream ifile (argv[1]); // Open file to read
  if (ifile.fail()) {
    std::cout << "***Failed to open file***";
    return EXIT_FAILURE;
  }

  // Read text seg
  std::ws(ifile);
  std::getline(ifile, istring);
  ilist.push_back(IAP(istring, addrs));
  std::ws(ifile);
  readText(ifile, istring, ilist, smap, addrs, gp); // Returns after reading line with ".data"
  std::getline(ifile, istring);
  CharSep ddelim(" ,");
  Tokenizer tokstring{istring, ddelim};
  Tokenizer::iterator tokiter = tokstring.begin();
  std::string tok;
  readData(ifile, istring, tok, ilist, smap, addrs, tokstring, tokiter);
  ifile.close();
  ilist.pop_front();    //remove ".text"

  uint32_t objcode{0};
  CharSep idelim{" ,:()"};
  INLIST::iterator liter = ilist.begin();
  tokstring.assign(liter->first, idelim);
  tokiter = tokstring.begin();
  OMAP::iterator oiter;
  RMAP::iterator riter;
  uint8_t second{0};
  std::string fArray[] = {"", "", "", "", "", ""};
  bool label = false;

  for (; liter != ilist.end(); ++liter) {
    if (liter->first == ".data") // Break at encountering .data
      break;
    if (liter->first.back() == ':') // Skip label
      continue;
    tokstring.assign(liter->first);
    tokiter = tokstring.begin();
    tok = *tokiter;              // 1st token == Opcode/Funct
    oiter = opmap.find(tok);
    for (auto& i : fArray)
      i = "";
    fArray[opfn] = tok;
    second = oiter->second;
    if ((second & 0x80) == 0) { // R FORMAT
      if (second == 0x0c) { // SYSCALL
        objcode |= second;
        oblist.push_back(objcode);
        objcode = 0;
        continue;
      } else if ((second & 0xf0) == 0x20) { //!= 0x0c) // NORMAL R i.e 3 registers
        fArray[rd] = *(++tokiter);
        fArray[rs] = *(++tokiter);
        fArray[rt] = *(++tokiter);
      } else if (second == 0x1a || second == 0x18) { // MULT AND DIV
        fArray[rs] = *(++tokiter);
        fArray[rt] = *(++tokiter);
        fArray[rd] = "";
      } else { //if (second == 0x10 || second == 0x12)   // MFHI AND MFLO
        fArray[rd] = *(++tokiter);
        fArray[rs] = "";
        fArray[rt] = "";
      }
      fArray[shmt] = "";
      fArray[lim] = "";
      assembleR(fArray, regmap, second, objcode); // ASSEMBLER R
    }    // END IF -- END R FORMAT ASSEMBLY
    else { // START ASSEMBLING I FORMAT
      //std::cout << "REACHING I FORMAT " << std::hex << (second >> 6) << "\n"; //TEST
      second = second << 1 >> 1;
      objcode = (objcode | second) << 5; // INSERT THE OPCODE
      if (tok != "j") { // LABEL INVOLED INSTRUCTION
        if (tok == "bne" || tok == "beq") { // HANDLE BRANCHES
          fArray[rs] = *(++tokiter);
          fArray[rt] = *(++tokiter);
          fArray[lim] = *(++tokiter);
          label = true;
        } else if (tok == "addiu") { // LW AND SW
          fArray[rt] = *(++tokiter);
          fArray[rs] = *(++tokiter);
          fArray[lim] = *(++tokiter);
          label = false;
        } else { // LW SW
          fArray[rt] = *(++tokiter);
          fArray[lim] = *(++tokiter);
          fArray[rs] = *(++tokiter);
          if (isdigit(fArray[lim].front())) {
            label = false;
          } else { // LW SW USIGN LABEL
            label = true;
          }
        }// END EVERY I ACCEPT J
      } else { // J Format
        objcode <<= 21;
        tok = *(++tokiter);
        objcode |= smap.find(tok)->second;
        oblist.push_back(objcode);
        objcode = 0;
        continue;
      }
      fArray[rd] = "";
      fArray[shmt] = "";
      assembleI(fArray, regmap, smap, label, gp, objcode, liter);
    } // END ELSE -- END I FORMAT ASSEMBLY
    oblist.push_back(objcode);
    objcode = 0;
  }     // END FOR -- INSTRUCTION SEGMENT
  // DATA SEGMENT
  ++liter;
  while (liter != ilist.end()) {
    tokstring.assign(liter->first);
    tokiter = tokstring.begin();
    ++tokiter;
    assembleData(tokstring, tokiter, oblist, objcode);
    objcode = 0;
    ++liter;
  }

  std::cout << "\n";
  for (auto& i : oblist)
    std::cout << std:: setw(8) << std::setfill('0') << std::hex
              << i << "\n";
}

/*************************
   FUNCTION DEFINITIONS
**************************/

void initOpTable (OMAP& opmap) {
  opmap["addiu"] = 0x89;
  opmap["addu"] = 0x21;
  opmap["and"] = 0x24;
  opmap["beq"] = 0x84; // op = 0x04
  opmap["bne"] = 0x85; // op = 0x05
  opmap["div"] = 0x1a;
  opmap["j"] = 0x82;   // op = 0x02
  opmap["lw"] = 0xa3;  // op = 0x23
  opmap["mfhi"] = 0x10;
  opmap["mflo"] = 0x12;
  opmap["mult"] = 0x18;
  opmap["or"] = 0x25;
  opmap["slt"] = 0x2a;
  opmap["subu"] = 0x23;
  opmap["sw"] = 0xab;       // op = 0x2b
  opmap["syscall"] = 0x0c;  // R form special
}

void initRegTable (RMAP& regmap) {
  regmap["$zero"] = 0;
  regmap["$v0"] = 2;
  regmap["$v1"] = 3;
  regmap["$a0"] = 4;
  regmap["$a1"] = 5;  //5
  regmap["$a2"] = 6;
  regmap["$a3"] = 7;
  regmap["$t0"] = 8;
  regmap["$t1"] = 9;
  regmap["$t2"] = 10;  //10
  regmap["$t3"] = 11;
  regmap["$t4"] = 12;
  regmap["$t5"] = 13;
  regmap["$t6"] = 14;
  regmap["$t7"] = 15;  //15
  regmap["$t8"] = 24;
  regmap["$t9"] = 25;
  regmap["$s0"] = 16;
  regmap["$s1"] = 17;
  regmap["$s2"] = 18;  //20
  regmap["$s3"] = 19;
  regmap["$s4"] = 20;
  regmap["$s5"] = 21;
  regmap["$s6"] = 22;
  regmap["$s7"] = 23;  //25
  regmap["$gp"] = 28;
}

void readText (std::ifstream& ifile, std::string& istring, INLIST& ilist, SMAP& smap, size_t& addrs, size_t& gp) {
  while (std::getline(ifile, istring)) {
    ilist.push_back(IAP(istring, addrs));
    if (istring.back() == ':') { // If lable, add to symbol table
      istring.pop_back(); // Remove : from label
      smap[istring] = addrs;
      continue;
    }
    if (istring == ".data") { // Get address for $gp
      gp = addrs;
      std::ws(ifile);
      return;
    }
    ++addrs;
    std::ws(ifile);
  }
}

void readData (std::ifstream& ifile, std::string& istring, std::string& tok, INLIST& ilist, SMAP& smap, size_t& addrs,
               Tokenizer& tokstring, Tokenizer::iterator& tokiter) {
  do { // Read data segment
    tokstring.assign(istring);
    ilist.push_back(IAP(istring, addrs)); // STILL PUSH IN LIST
    tokiter = tokstring.begin();
    tok = *tokiter;
    tok.pop_back();         // REMOVE : IN LABEL
    smap[tok] = addrs;     // ADD LABEL AND ADDRESS TO SYM TAB FOR QUICK LOOK UP LATER
    tok = *(++tokiter);   // CHECK IF .WORD OR .SPACE
    ++tokiter;
    if (tok == ".word") {
      while (tokiter != tokstring.end()) {
        addrs += 4;  // ASSIGN A WORD SPACE
        ++tokiter;
      }
    } else {
      while (tokiter != tokstring.end()) {
        ++addrs;     // ASSIGN BYTE SPACE
        ++tokiter;
      }
    }
    std::ws(ifile);
  } while (std::getline(ifile, istring));
}

void assembleR (std::string fArray[], const RMAP& regmap, const uint8_t& funct, uint32_t& objcode) {
  for (size_t i = rs; i != lim; ++i) {
    if (fArray[i] != "") { //ASSUMES VALID REGISTER AND IN REG MAP
      objcode |= regmap.find(fArray[i])->second;
    }
    objcode <<= 5;
  }
  objcode = (objcode << 1) | funct;
}

void assembleI (std::string fArray[], const RMAP& regmap, const SMAP& smap, bool label,
                const size_t& gp, uint32_t& objcode, const INLIST::iterator& liter) {
  size_t i{0};
  for (i = rs; i != rd; ++i) {
    if (fArray[i] != "") { //ASSUMES VALID REGISTER AND IN REG MAP
      objcode |= regmap.find(fArray[i])->second;
    }
    objcode <<= 5;
  }
  objcode <<= 11;
  if (label) {
    i = smap.find(fArray[lim])->second;
    if (i < gp) { // branches
      i -= liter->second;
      --i;
    } else {
      i -= gp;
    }
  } else { // addiu and reg sw lw
    i = std::stoi(fArray[lim]);
  }
  objcode |= (0x0000ffff & i);
}

void assembleData (const Tokenizer& tokstring, Tokenizer::iterator& tokiter, OBLIST& oblist, uint32_t& objcode) {
  if (*tokiter == ".word") {
    ++tokiter;
    for (; tokiter != tokstring.end(); ++tokiter) {
      objcode = std::stoi(*tokiter);
      oblist.push_back(objcode);
    }
  } else {
    ++tokiter;
    int i = std::stoi(*tokiter);
    for (int j = 0; j != i; ++j)
      oblist.push_back(objcode);
  }
}
