#include "scvaltypes.h"
#include <string.h>
#include <stdio.h>
#include <locale>

#define SAFEFREE(arp) { if ( arp ){ free((void*)(arp)); (arp)=0; } }
//===---------------------------------------------------------------------------===//
//===---------------------------------------------------------------------------===//
void ScvalVMContext::Clear()
{
  SAFEFREE(m_regCounters);  
  SAFEFREE(m_regStrHashes);
  for ( int i=0; i < m_stringCount; ++i )
    SAFEFREE(m_regStrings[i]);
  SAFEFREE(m_regStrings);
  m_cmpRes = 0;
  m_stringCount=0;
}
void ScvalVMContext::Init( int regC, int regS )
{
  Clear();
  m_regCounters = (unsigned short*)calloc(regC+1,sizeof(unsigned short));
  m_regStrHashes = (ScvalHashID*)calloc(regS+1,sizeof(ScvalHashID));
  m_regStrings = (const char**)calloc(regS+1,sizeof(const char*));
  m_stringCount = regS+1;
}
//===---------------------------------------------------------------------------===//
//===---------------------------------------------------------------------------===//
ScvalVMCode::ScvalVMCode()
  : m_maxRegCounter(0), m_maxRegStrings(0), m_code(0)
  , m_noOperations(0), m_constData(0), m_noConstData(0)
{}
void ScvalVMCode::Clear()
{
  SAFEFREE(m_code);
  SAFEFREE(m_constData);
  m_maxRegCounter = m_maxRegStrings = 0;
  m_noOperations = m_noConstData = 0;
}
//===---------------------------------------------------------------------------===//
//===---------------------------------------------------------------------------===//
void ScvalVM::Clear()
{
  m_mainCtx.Clear();
}
bool ScvalVM::Run( const ScvalVMCode* code, ScvalInstHook* hook )
{
  m_mainCtx.Init( code->m_maxRegCounter, code->m_maxRegStrings );
  m_pc = m_opExecuted = 0;
  register int& CMPRES = m_mainCtx.m_cmpRes;
  ScvalHashID* R_HASHES = m_mainCtx.m_regStrHashes;
  const char** R_STRS = m_mainCtx.m_regStrings;
  unsigned short* R_CNTS = m_mainCtx.m_regCounters;
  const unsigned int maxPC = code->m_noOperations;
  while ( m_pc < maxPC )
  {
    register const ScvalVMOperation& operation = code->m_code[m_pc++];
    m_opExecuted++;
    switch ( operation.opcode )
    {
    case VM_LDEN: 
    case VM_LDEV: 
    case VM_LDAN: 
    case VM_LDAV: 
      {
        const char* retStr = hook->Do( (ScvalVMOpcode)operation.opcode );
        R_HASHES[operation.op0] = ScvalHash( retStr ); 
        SAFEFREE(R_STRS[operation.op0]);
        R_STRS[operation.op0] = _strdup(retStr);
      }break;    
    case VM_CMPS: 
      {
        unsigned int dataAddr = operation.GetDataAddr();
        const int op2 = dataAddr==VM_NILDATA ? 0 : code->m_constData[dataAddr];
        CMPRES = R_HASHES[operation.op0] - op2; 
      }break;
    case VM_CMPI: 
      CMPRES = R_CNTS[operation.op0] - operation.op1; 
      R_CNTS[operation.op0]=0;
      break;    
    case VM_JE: 
      if ( CMPRES == 0 )
        m_pc = operation.GetAddr();
      break;
    case VM_JNE:
      if ( CMPRES != 0 )
        m_pc = operation.GetAddr();
      break;
    case VM_JG: 
      if ( CMPRES > 0 )
        m_pc = operation.GetAddr();
      break;
    case VM_JMP:
      m_pc = operation.GetAddr();
    case VM_CLR: break;
    case VM_INC: 
      R_CNTS[operation.op0]++; 
      break;
    case VM_CHKN:
      switch ( operation.op1 ) // native type to check
      {
      case 0: if ( !IsReal(R_STRS[operation.op0]) ) m_pc = VM_ERRADDR; break;
      case 1: break;
      case 2: if ( !IsInteger(R_STRS[operation.op0]) ) m_pc = VM_ERRADDR; break;
      case 3: if ( !IsBool(R_HASHES[operation.op0] ) ) m_pc = VM_ERRADDR; break;
      }break;
    case VM_CHKC:
      m_mainCtx.m_checkStrReg = operation.op0;
      m_lastPc = m_pc; // stack of 1 level of depth
      m_pc = operation.GetDataAddr();// in chkc operation, only 2 bytes for address
      break;
    case VM_DOWN: 
    case VM_UP  : 
    case VM_GATT: 
    case VM_NATT: 
    case VM_NEXT: 
      hook->Do( (ScvalVMOpcode)operation.opcode ); 
      break;
    case VM_RET:
      m_pc = m_lastPc;
      break;
    case VM_CALL:
      CMPRES = int(hook->Do( (ScvalVMOpcode)operation.opcode, code->m_constData[operation.GetDataAddr()], R_STRS[m_mainCtx.m_checkStrReg] ));
      break;
    default: return false;
    }
  }
  printf( "%d instructions executed\n", m_opExecuted );
  // this special pc address is considered that there was an error
  return m_pc != VM_ERRADDR;
}
bool ScvalVM::IsInteger( const char* str )
{
  while ( *str && isdigit(*str++) ) 
  {}
  return !*str;
}
bool ScvalVM::IsReal( const char* str )
{
  int nopoints=0;
  do
  {
    while ( *str && isdigit(*str++) ) 
    {}
    if ( *str == '.' ) nopoints++;
  }while (*str && nopoints<=1);
  return !*str && nopoints<=1; 
}
static const ScvalHashID g_hTrue = ScvalHash("true");
static const ScvalHashID g_hFalse= ScvalHash("false");
static const ScvalHashID g_h0= ScvalHash("0");
static const ScvalHashID g_h1= ScvalHash("1");
bool ScvalVM::IsBool( ScvalHashID strhash )
{
  return strhash == g_hTrue || strhash == g_hFalse || 
         strhash == g_h0    || strhash == g_h1;
}
//===---------------------------------------------------------------------------===//
//===---------------------------------------------------------------------------===//
bool ScvalValidate(const ScvalVMCode& inBytecode, ScvalInstHook* xmlReader )
{
  ScvalVM vm;
  return vm.Run( &inBytecode, xmlReader );
}

//===---------------------------------------------------------------------------===//
// Load the bytecode from binary chunk
//===---------------------------------------------------------------------------===//
bool ScvalLoadFromBinary( const void* binChunk, ScvalVMCode& outBytecode )
{
  outBytecode.Clear();
  unsigned int* ptr = (unsigned int*)binChunk;
  outBytecode.m_maxRegCounter = *ptr++;
  outBytecode.m_maxRegStrings = *ptr++;
  outBytecode.m_noOperations = *ptr++;
  outBytecode.m_noConstData = *ptr++;
  outBytecode.m_code = (ScvalVMOperation*)malloc( sizeof(ScvalVMOperation)*outBytecode.m_noOperations );
  if ( !outBytecode.m_code )
    return false;
  memcpy( outBytecode.m_code, ptr, sizeof(ScvalVMOperation)*outBytecode.m_noOperations );
  ptr += outBytecode.m_noOperations;

  outBytecode.m_constData = (ScvalHashID*)malloc( sizeof(ScvalHashID)*outBytecode.m_noConstData );
  if ( !outBytecode.m_constData )
    return false;
  memcpy( outBytecode.m_constData, ptr, sizeof(ScvalHashID)*outBytecode.m_noConstData );
  return true;
}

//===---------------------------------------------------------------------------===//
// Save the bytecode to binary chunk
//===---------------------------------------------------------------------------===//
bool ScvalSaveToBinary( const ScvalVMCode& inBytecode, void** outBinChunk, unsigned int& chunkSizeBytes )
{
  unsigned int size = sizeof(unsigned int)*4; // 2 m_max* + 2 m_no* data members
  // code and data segment size
  size += inBytecode.m_noOperations*sizeof(ScvalVMOperation) + inBytecode.m_noConstData*sizeof(ScvalHashID);
  chunkSizeBytes = size;
  *outBinChunk = malloc( chunkSizeBytes );
  if ( !*outBinChunk ) 
    return false;
  unsigned int* ptr = (unsigned int*)(*outBinChunk);
  *ptr++ = inBytecode.m_maxRegCounter;
  *ptr++ = inBytecode.m_maxRegStrings;
  *ptr++ = inBytecode.m_noOperations;
  *ptr++ = inBytecode.m_noConstData;
  memcpy( ptr, inBytecode.m_code, sizeof(ScvalVMOperation)*inBytecode.m_noOperations );
  ptr += inBytecode.m_noOperations;
  memcpy( ptr, inBytecode.m_constData, sizeof(ScvalHashID)*inBytecode.m_noConstData );
  return true;
}

//===---------------------------------------------------------------------------===//
//===---------------------------------------------------------------------------===//
void ScvalBinaryDeallocate( void** binChunk )
{
  SAFEFREE(*binChunk);
}


#undef SAFEFREE