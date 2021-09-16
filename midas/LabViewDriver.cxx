//
// LabViewDriver
//
// Frontend for interfacing with LabView over TCP
//

#include <stdio.h>
#include <signal.h> // SIGPIPE
#include <assert.h> // assert()
#include <stdlib.h> // malloc()
#include <iostream>
#include <iomanip>              // to change stream formatting
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <stdexcept>
#include <set>

#include "midas.h"
#include "tmfe.h"

#include "KOtcp.h"
#include "feTCP.h"

using std::string;
using std::vector;
using std::cout;
using std::cerr;
using std::endl;

#define NCH 12
#define SEPARATOR "_"

/**
 * \brief helper function to split a string into a vector of strings
 */
vector<string> split(const string& str, const string& delim)
{
    vector<string> tokens;
    size_t prev = 0, pos = 0;
    do
    {
        pos = str.find(delim, prev);
        if (pos == string::npos) pos = str.length();
        string token = str.substr(prev, pos-prev);
        if (!token.empty()) tokens.push_back(token);
        prev = pos + delim.length();
    }
    while (pos < str.length() && prev < str.length());
    return tokens;
}

int add_key(HNDLE hDB, HNDLE hkey, KEY *key, INT level, void *pvector){
   if(key->type != TID_KEY)
      ((vector<KEY>*)pvector)->push_back(*key);
   return DB_SUCCESS;
}

/**
 * \brief Generic class for TCP/IP communication with LabView server program.
 * Requests list of readable and writable variables from LabView, then populates ODB
 *
 * The idea behind this is that a change in hardware requires only modification of the LabView
 * program, not the Midas frontend. If LabView adds a new variable since last connection, it will
 * be added to the ODB. For safety reasons if a variable or setting gets removed from LabView, it does
 * not automatically get removed from the ODB, just a warning message is issued.
 *
 * Some ODB settings are always present, not dependent on the configuration of the LabView server:
 * @param hostname IP or hostname of the LabView server
 * @param port port LabView is listening on
 * @param applyOnFestart \c true to overwrite LabView settings with ODB values, \c false (default) update ODB with current settings from LabView <b>NOT IMPLEMENTED YET</b>
 */
class feLabview :
   public feTCP
{
public:
   feLabview(TMFE* mfe /**< [in]  Midas frontend interfacing class. */,
             TMFeEquipment* eq /**< [in]  Midas equipment class. */):feTCP(mfe, eq) // ctor
   {
      fMfe = mfe;
      fEq  = eq;
      fEventSize = 0;
      fEventBuf  = NULL;
   }

   /** \brief Variable initialization. */
   void Init()
   {
      // fEventSize = 100;
      // fEq->fOdbEqSettings->RI("event_size", &fEventSize, true);
      // if (fEventBuf) {
      //    free(fEventBuf);
      // }
      // fEventBuf = (char*)malloc(fEventSize);

      // add ODB settings that are internal -> not for LabView
      fEq->fOdbEqSettings->RI("verbosity", &verbose, true);

      sets.push_back("hostname");
      stype.push_back(TID_STRING);
      sets.push_back("port");
      stype.push_back(TID_STRING);
      sets.push_back("verbosity");
      stype.push_back(TID_INT);
      sets.push_back("applyOnFestart");
      stype.push_back(TID_BOOL);

      nFixedSettings = sets.size();
      nFixedVars = vars.size();
   }

   // /* \brief Midas event creation \b UNUSED. */
   // void SendEvent(double dvalue)
   // {
   //    fEq->ComposeEvent(fEventBuf, fEventSize);
   //    fEq->BkInit(fEventBuf, fEventSize);

   //    double* ptr = (double*)fEq->BkOpen(fEventBuf, "test", TID_DOUBLE);
   //    *ptr = dvalue;
   //    fEq->BkClose(fEventBuf, ptr+1);

   //    fEq->SendEvent(fEventBuf);
   // }

   // /* \brief JSON rpc interface <b>CURRENTLY UNUSED</b>. */
   // std::string HandleRpc(const char* cmd, const char* args)
   // {
   //    fMfe->Msg(MINFO, "HandleRpc", "RPC cmd [%s], args [%s]", cmd, args);
   //    return "OK";
   // }

   // /* \brief Begin-of-Run operations <b>CURRENTLY UNUSED</b>. */
   // void HandleBeginRun()
   // {
   //    fMfe->Msg(MINFO, "HandleBeginRun", "Begin run!");
   //    fEq->SetStatus("Running", "#00FF00");
   // }

   // /* \brief End-of-Run operations <b>CURRENTLY UNUSED</b>. */
   // void HandleEndRun()
   // {
   //    fMfe->Msg(MINFO, "HandleEndRun", "End run!");
   //    fEq->SetStatus("Stopped", "#00FF00");
   // }

   /** \brief Periodic operations, reading variables from LabView. */
   void HandlePeriodic()
   {
      // printf("periodic!\n");
      read_event();
      //char buf[256];
      //sprintf(buf, "buffered %d (max %d), dropped %d, unknown %d, max flushed %d", gUdpPacketBufSize, fMaxBuffered, fCountDroppedPackets, fCountUnknownPackets, fMaxFlushed);
      //fEq->SetStatus(buf, "#00FF00");
      //fEq->WriteStatistics();
   }

   /** \brief Function called on ODB setting change, sending variables to LabView. */
   void fecallback(HNDLE hDB, HNDLE hkey, INT index);
   INT read_event();

   /** \brief Connect to LabView and confirm identity. */
   bool LVConnect()
   {
      bool success = TCPConnect();
      if(success) success = Handshake();
      return success;
   }

   /** \brief Request list of variables from LabView, populate ODB. */
   unsigned int GetVars();
private:
   /** \brief Confirm server we're talking to is actually LabView. */
   bool Handshake(){
      string resp = Exchange("midas\r\n");
      bool correct = (resp.substr(0,7) == string("labview"));
      if(!correct) fMfe->Msg(MERROR, "Handshake", "Unexpected response: %s", resp.c_str());
      else if(verbose) fMfe->Msg(MINFO, "Handshake", "Handshake successful");
      return correct;
   }
   int TypeConvert(const char c);
   char TypeConvert(const int t);
   enum varset { var, set };
   template <class T>
   bool ReadLVVar(const varset vs, const string name, const int type, T &retval);
   bool WriteLVSetFromODB(const HNDLE hkey, bool confirm = true);
   template <class T>
   bool WriteLVSet(const string name, const int type, const T val, bool confirm = true);
   template <class T>
   void WriteODB(const varset vs, const string name, const int type, const T val);
   void WriteODB(const varset vs, const string name, const int type, const string val);
   template <class T>
   void ReadODB(const varset vs, const string name, const int type, T &retval);
   void ReadODB(const varset vs, const string name, const int type, string &retval);
   vector<string> vars, sets;
   vector<int> vtype, stype;
   unsigned int nFixedSettings, nFixedVars;
   int verbose = 1;
};

/** \brief global wrapper for Midas callback of class function
 *
 */
void callback(INT hDB, INT hkey, INT index, void *feptr)
{
   feLabview* fe = (feLabview*)feptr;
   fe->fecallback(hDB, hkey, index);
}

int feLabview::TypeConvert(const char ctype)
{
   switch(ctype){
   case 'B': return TID_BOOL; break;
   case 'I': return TID_INT; break;
   case 'F': return TID_FLOAT; break;
   case 'D': return TID_DOUBLE; break;
   case 'S': return TID_STRING; break;
   case 'u': return TID_WORD; break;
   case 'U': return TID_DWORD; break;
   default : fMfe->Msg(MERROR, "TypeConvert", "Unsupported data type: %c",  ctype); return 0;
   }
}

char feLabview::TypeConvert(const int type)
{
   switch(type){
   case TID_BOOL:   return 'B'; break;
   case TID_INT:    return 'I'; break;
   case TID_FLOAT:  return 'F'; break;
   case TID_DOUBLE: return 'D'; break;
   case TID_STRING: return 'S'; break;
   case TID_WORD:   return 'u'; break;
   case TID_DWORD:  return 'U'; break;
   default: return 'X';         // this shouldn't occur
   }
}

template <class T>
bool feLabview::ReadLVVar(const varset vs, const string name, const int type, T &retval)
{
   std::ostringstream oss;
   if(vs == var) oss << 'R';
   else if(vs == set) oss << 'W';
   oss << TypeConvert(type);
   oss << name;
   string varname = oss.str();
   oss << "_?\r\n";
   string resp=Exchange(oss.str(), true, varname);
   if(verbose>2) cout << "ReadLVVar Sent: " << oss.str() << "\tReceived: " << resp << endl;
   vector<string> rv = split(resp, SEPARATOR);
   if(rv.size()==2){
      if(rv[0] != varname){
         cm_msg(MERROR, "ReadLVVar", "Asked for %s, but got %s", name.c_str(), rv[0].c_str());
         return false;
      }
      std::istringstream iss(rv[1]);
      if(iss >> retval) return true;    // this acts like a boolean, so if the operation fails, returns false
      else return false;
   }
   return false;
}

bool feLabview::WriteLVSetFromODB(const HNDLE hkey, bool confirm)
{
   MVOdb *db = fEq->fOdbEqSettings;
   KEY key;
   int status = db_get_key(fMfe->fDB, hkey, &key);
   bool success = !confirm;
   if(status == DB_SUCCESS){    // Use template functions here
      if(verbose > 1){
         std::cout << "Setting ODB entry " << key.name << std::endl;
      }
      switch(key.type){
      case TID_BOOL:{
         bool val;
         db->RB(key.name, &val);
         success = WriteLVSet(key.name, key.type, val, confirm);
         break;
      }
      case TID_INT:{
         int val;
         db->RI(key.name, &val);
         success = WriteLVSet(key.name, key.type, val, confirm);
         break;
      }
      case TID_FLOAT:{
         float val;
         db->RF(key.name, &val);
         success = WriteLVSet(key.name, key.type, val, confirm);
         break;
      }
      case TID_DOUBLE:{
         double val;
         db->RD(key.name, &val);
         success = WriteLVSet(key.name, key.type, val, confirm);
         break;
      }
      case TID_STRING:{
         string val;
         db->RS(key.name, &val);
         success = WriteLVSet(key.name, key.type, val, confirm);
         break;
      }
      case TID_WORD:{
         uint16_t val;
         db->RU16(key.name, &val);
         success = WriteLVSet(key.name, key.type, val, confirm);
         break;
      }
      case TID_DWORD:{
         uint32_t val;
         db->RU32(key.name, &val);
         success = WriteLVSet(key.name, key.type, val, confirm);
         break;
      }
      }
      return success;
   } else {
      return false;
   }
}

template <class T>
bool feLabview::WriteLVSet(const string name, const int type, const T val, bool confirm)
{
   std::ostringstream oss;
   oss << 'W';
   oss << TypeConvert(type);
   oss << name << '_';
   if(type == TID_FLOAT || type == TID_DOUBLE){ // FIXME: hack because currently Labview doesn't know how to read scientific notation
      oss << std::fixed << std::setprecision(16);
   }
   oss << val << "\r\n";
   if(verbose > 1){
      cout << "Sending: " << oss.str() << endl;
   }
   Exchange(oss.str(), false);
   if(confirm){
      usleep(100000);
      T retval;
      verbose++;
      bool result = ReadLVVar(set, name, type, retval); // FIXME: Readback fails somehow
      int count = 0;
      while(!result && count < 3){
         if(verbose > 1) cerr << "Readback failed, trying again..." << count << endl;
         result = ReadLVVar(set, name, type, retval); // FIXME: second attempt usually works
         count++;
      }
      verbose--;
      if(!result){
         cm_msg(MERROR, "WriteLVSet", "Readback for %s failed\n", name.c_str());
      } else if(retval != val){
         std::ostringstream oss2;
         oss2 << "Readback for " << name << " doesn't match request: " << retval << " != " << val;
         cm_msg(MERROR, "WriteLVSet", "%s", oss2.str().c_str());
      } else if(verbose>1){
         cout << "WriteLVSet Sent: " << oss.str() << "\tReceived: " << retval << endl;
      }
      return result;
   } else {
      return true;
   }
}

template <class T>
void feLabview::WriteODB(const varset vs, const string name, const int type, const T val)
{
   MVOdb *db = fEq->fOdbEqVariables;
   MVOdbError err;
   if(vs == set) db = fEq->fOdbEqSettings;
   assert(type != TID_STRING);
   if(verbose > 2){
      cout << "Writing to ODB: " << name << "\tvalue: " << val << endl;
   }
   switch(type){
   case TID_BOOL:   db->WB(name.c_str(), val, &err); break;
   case TID_INT:    db->WI(name.c_str(), val, &err); break;
   case TID_FLOAT:  db->WF(name.c_str(), val, &err); break;
   case TID_DOUBLE: db->WD(name.c_str(), val, &err); break;
   case TID_WORD:   db->WU16(name.c_str(), val, &err); break;
   case TID_DWORD:  db->WU32(name.c_str(), val, &err); break;
   default: assert(0);          // Die if unsupported type is requested
   }
   if(err.fError){
      cerr << "ERROR!!! " << err.fErrorString << "Status: " << err.fStatus << endl;
   }
}

void feLabview::WriteODB(const varset vs, const string name, const int type, const string val)
{
   MVOdb *db = fEq->fOdbEqVariables;
   if(vs == set) db = fEq->fOdbEqSettings;
   assert(type == TID_STRING);
   if(verbose > 2){
      cout << "Writing string to ODB: " << name << "\tvalue: " << val << endl;
   }
   db->WS(name.c_str(), val.c_str(), val.size());
}

template <class T>
void feLabview::ReadODB(const varset vs, const string name, const int type, T &val)
{
   MVOdb *db = fEq->fOdbEqVariables;
   if(vs == set) db = fEq->fOdbEqSettings;
   assert(type != TID_STRING);
   switch(type){
   case TID_BOOL:   db->RB(name.c_str(), val); break;
   case TID_INT:    db->RI(name.c_str(), val); break;
   case TID_FLOAT:  db->RF(name.c_str(), val); break;
   case TID_DOUBLE: db->RD(name.c_str(), val); break;
   case TID_WORD:   db->RU16(name.c_str(), val); break;
   case TID_DWORD:  db->RU32(name.c_str(), val); break;
   }
}

void feLabview::ReadODB(const varset vs, const string name, const int type, string &val)
{
   MVOdb *db = fEq->fOdbEqVariables;
   if(vs == set) db = fEq->fOdbEqSettings;
   assert(type == TID_STRING);
   db->RS(name.c_str(), &val);
}

unsigned int feLabview::GetVars()
{
   sets.resize(nFixedSettings); vars.resize(nFixedVars);
   stype.resize(nFixedSettings); vtype.resize(nFixedVars);
   string resp = Exchange("list_vars\r\n");
   cout << "Response: " << resp << "(" << resp.size() << ")" << endl;
   vector<string> tokens = split(resp, SEPARATOR);
   for(string s: tokens){
      char set_or_var = s[0];
      char ctype = s[1];
      s.erase(0,2);
      int type = TypeConvert(ctype);
      if(set_or_var == 'W'){
         sets.push_back(s);
         stype.push_back(type);
      } else if(set_or_var == 'R'){
         vars.push_back(s);
         vtype.push_back(type);
      }
   }
   vector<string> odbsets, odbvars;
   vector<int> odbstid, odbvtid;
   vector<KEY> odbsetkeys, odbvarkeys;
   char tmpbuf[80];
   sprintf(tmpbuf, "/Equipment/%s/Settings", fMfe->fFrontendName.c_str());
   HNDLE odbs, odbv;
   db_find_key(fMfe->fDB, 0, tmpbuf, &odbs);
   sprintf(tmpbuf, "/Equipment/%s/Variables", fMfe->fFrontendName.c_str());
   db_find_key(fMfe->fDB, 0, tmpbuf, &odbv);
   db_scan_tree(fMfe->fDB, odbs, 0, add_key, (void*)&odbsetkeys);
   db_scan_tree(fMfe->fDB, odbv, 0, add_key, (void*)&odbvarkeys);
   for(KEY key: odbsetkeys){
      odbsets.push_back(key.name);
      odbstid.push_back(key.type);
   }
   for(KEY key: odbvarkeys){
      odbvars.push_back(key.name);
      odbvtid.push_back(key.type);
   }
   // fEq->fOdbEqSettings->ReadDir(&odbsets, &odbstid, &odbsnum, &tsize, &isize); // FIXME: function not implememnted!
   // fEq->fOdbEqVariables->ReadDir(&odbvars, &odbvtid, &odbvnum, &tsize, &isize);
   if(verbose > 1){
      for(unsigned int i = 0; i < odbsets.size(); i++){
         cout << odbsets[i] << '\t' << odbstid[i] << endl;
      }
      for(unsigned int i = 0; i < odbvars.size(); i++){
         cout << odbvars[i] << '\t' << odbvtid[i] << endl;
      }
   }
   if(verbose){
      if(odbsets == sets) cout << "Settings match!" << endl;
      else {
         std::set<string> sodbsets(odbsets.begin(), odbsets.end()), ssets(sets.begin(), sets.end()); // Using set so they're ordered
         if(sodbsets == ssets) cout << "Settings match!" << endl;
         else {
            cout << "Settings don't match!" << endl;
            cout << "LabView:\t";
            for(string s: ssets) cout << s << '\t';
            cout <<  endl;
            cout << "ODB:    \t";
            for(string s: sodbsets) cout << s << '\t';
            cout <<  endl;
         }
      }
   }
   for(unsigned int i = 0; i < sets.size(); i++){
      bool found = false;
      if(!sets[i].size())
         cerr << "Empty sets string at pos " << i << endl;
      vector<string>::iterator it = std::find(odbsets.begin(), odbsets.end(), sets[i]);
      if(it != odbsets.end()){
         unsigned int j = distance(odbsets.begin(), it);
         if(stype[i] == odbstid[j]) found = true;
         else {
            fMfe->Msg(MERROR, "GetVars", "Key %s exists, but has wrong type: %d instead of %d. Delete key manually to generate correct type.", sets[i].c_str(), odbstid[j], stype[i]);
            exit(DB_TYPE_MISMATCH);
            // don't want to delete keys automatically
         }
      }
      if(!found){
         db_create_key(fMfe->fDB, odbs, sets[i].c_str(), stype[i]);
      }
   }
   for(unsigned int i = 0; i < vars.size(); i++){
      bool found = false;
      if(!vars[i].size())
         cerr << "Empty vars string at pos " << i << endl;
      vector<string>::iterator it = std::find(odbvars.begin(), odbvars.end(), vars[i]);
      if(it != odbvars.end()){
         unsigned int j = distance(odbvars.begin(), it);
         if(vtype[i] == odbvtid[j]) found = true;
         else {
            fMfe->Msg(MERROR, "GetVars", "Key %s exists, but has wrong type: %d instead of %d. Delete key manually to generate correct type.", vars[i].c_str(), odbvtid[j], vtype[i]);
            exit(DB_TYPE_MISMATCH);
            // don't want to delete keys automatically
         }
      }
      if(!found){
         db_create_key(fMfe->fDB, odbv, vars[i].c_str(), vtype[i]);
      }
   }
   int orphans = 0;
   for(unsigned int i = nFixedSettings; i < odbsets.size(); i++){
      vector<string>::iterator it = std::find(sets.begin(), sets.end(), odbsets[i]);
      if(it == sets.end()){
         orphans++;
         fMfe->Msg(MINFO, "GetVars", "Orphaned key: Setting %s does not match available LabView settings", odbsets[i].c_str());
         // message
      } else {
         // create hotlink
         HNDLE hkey;
         int status = db_find_key(fMfe->fDB, odbs, sets[i].c_str(), &hkey);
         if (status != DB_SUCCESS) {
            cm_msg(MERROR, "Init", "Key not found: %s", sets[i].c_str());
         }
         db_watch(fMfe->fDB, hkey, callback, (void*)this);
      }
   }
   for(string s: odbvars){
      vector<string>::iterator it = std::find(vars.begin(), vars.end(), s);
      if(it == vars.end()){
         orphans++;
         fMfe->Msg(MINFO, "GetVars", "Orphaned key: Variable %s does not match available LabView variables", s.c_str());
         // message
      }
   }
   if(orphans)
      fMfe->Msg(MINFO, "GetVars", "Orphaned keys in ODB found: %d", orphans);
   return tokens.size();
}

void feLabview::fecallback(HNDLE hDB, HNDLE hkey, INT index)
{
   WriteLVSetFromODB(hkey, true);
}


INT feLabview::read_event()
{
   for(unsigned int i = 0; i < vars.size(); i++){
      std::ostringstream oss;
      oss << 'R';
      switch(vtype[i]){
      case TID_BOOL:
         {
            bool val;
            ReadLVVar(var, vars[i], vtype[i], val);
            WriteODB(var, vars[i], vtype[i], val);
            break;
         }
      case TID_INT:
         {
            int val;
            ReadLVVar(var, vars[i], vtype[i], val);
            WriteODB(var, vars[i], vtype[i], val);
            break;
         }
      case TID_DOUBLE:
         {
            double val;
            ReadLVVar(var, vars[i], vtype[i], val);
            WriteODB(var, vars[i], vtype[i], val);
            break;
         }
      case TID_FLOAT:
         {
            float val;
            ReadLVVar(var, vars[i], vtype[i], val);
            WriteODB(var, vars[i], vtype[i], val);
            break;
         }
      case TID_STRING:
         {
            string val;
            ReadLVVar(var, vars[i], vtype[i], val);
            WriteODB(var, vars[i], vtype[i], val);
            break;
         }
      case TID_WORD:
         {
            uint16_t val;
            ReadLVVar(var, vars[i], vtype[i], val);
            WriteODB(var, vars[i], vtype[i], val);
            break;
         }
      case TID_DWORD:
         {
            uint32_t val;
            ReadLVVar(var, vars[i], vtype[i], val);
            WriteODB(var, vars[i], vtype[i], val);
            break;
         }
      }
   }
   return 0;
}

static void usage()
{
   fprintf(stderr, "Usage: LabViewDriver_tmfe.exe <name> ...\n");
   exit(1);
}

int main(int argc, char* argv[])
{
   // setbuf(stdout, NULL);
   // setbuf(stderr, NULL);

   signal(SIGPIPE, SIG_IGN);

   std::string name = "";

   if (argc == 2) {
      name = argv[1];
   } else {
      usage(); // DOES NOT RETURN
   }

   TMFE* mfe = TMFE::Instance();

   TMFeError err = mfe->Connect(name.c_str(), __FILE__);
   if (err.error) {
      printf("Cannot connect, bye.\n");
      return 1;
   }

   //mfe->SetWatchdogSec(0);

   TMFeCommon *common = new TMFeCommon();
   common->EventID = 1;
   common->LogHistory = 1;
   //common->Buffer = "SYSTEM";

   TMFeEquipment* eq = new TMFeEquipment(mfe, name.c_str(), common);
   eq->Init();
   eq->SetStatus("Starting...", "white");
   eq->ZeroStatistics();
   eq->WriteStatistics();

   mfe->RegisterEquipment(eq);

   feLabview *myfe = new feLabview(mfe, eq);
   mfe->RegisterRpcHandler(myfe);

   //mfe->SetTransitionSequenceStart(910);
   //mfe->SetTransitionSequenceStop(90);
   //mfe->DeregisterTransitionPause();
   //mfe->DeregisterTransitionResume();

   myfe->Init();

   eq->SetStatus("Started...", "white");

   bool connected = myfe->LVConnect();
   if(!connected){
      cm_msg(MERROR, "TCPConnect", "Could not connect to host: %s:%s", myfe->fHostname.c_str(), myfe->fPortnum.c_str());
   } else {
      myfe->GetVars();
   }
   // char rot_set_str[80];
   // HNDLE hkey;
   // sprintf(rot_set_str, "/Equipment/%s/Settings/Rotation_position", name.c_str());
   // int status = db_find_key(mfe->fDB, 0, rot_set_str, &hkey);
   // if (status != DB_SUCCESS) {
   //    cm_msg(MERROR, "Init", "Key not found: %s", rot_set_str);
   // }
   // db_watch(mfe->fDB, hkey, callback, (void*)myfe);

   mfe->RegisterPeriodicHandler(eq, myfe);


   if(connected){
      std::ostringstream oss;
      oss << "Connected to " << myfe->fHostname << ':' << myfe->fPortnum;
      eq->SetStatus(oss.str().c_str(), "lightgreen");

      while (!mfe->fShutdownRequested) {
         mfe->PollMidas(10);
      }
   }
   mfe->Disconnect();

   return 0;
}

/* emacs
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 3
 * indent-tabs-mode: nil
 * End:
 */
