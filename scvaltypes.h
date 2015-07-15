#ifndef _SCVALTYPES_H_
#define _SCVALTYPES_H_
//===---------------------------------------------------------===//
// Static and dynamic arrays. Remains in stack memory when no
// elements further the STATIC_ELEMENTS threshold are needed
//===---------------------------------------------------------===//
template<typename T, int STATIC_ELEMENTS, int DYNAMIC_ELEMENTS_GRANULARITY>
class ScvalStaticDynArray
{
public:
  enum { STATIC_SIZE=STATIC_ELEMENTS };
  ScvalStaticDynArray():m_dynamicChunk(0), m_dynCapacity(0), m_size(0){}
  T& Create()
  {
    T* node;
    if ( m_size < STATIC_ELEMENTS )
    {
      node = m_staticChunk+(m_size++);
    }else
    {
      unsigned int relNdx = m_size-STATIC_ELEMENTS;
      // need to (re)allocate?
      if ( !m_dynamicChunk || relNdx >= m_dynCapacity )
      {
        // todo: apply a better growing policy (now is too greedy)
        int newCap = m_dynCapacity*2;
        if ( !newCap ) 
          newCap = DYNAMIC_ELEMENTS_GRANULARITY;
        T* newChunk = new T[newCap];
        if ( m_dynamicChunk )
        {
          // if there were elements, copy over
          memcpy( newChunk, m_dynamicChunk, sizeof(T)*m_dynCapacity );
          delete [] m_dynamicChunk;
        }
        m_dynamicChunk = newChunk;
        m_dynCapacity = newCap;      
      }
      node = m_dynamicChunk+relNdx;
      ++m_size;
    }
    return *node;
  }
  void RemoveBack()
  {
    if ( m_size > 0 )
      --m_size;
  }
  T& Get( unsigned int index )
  {
    return ( index < STATIC_ELEMENTS )
      ? m_staticChunk[index]
    : m_dynamicChunk[index-STATIC_ELEMENTS];
  }
  void Clear()
  {
    m_size = m_dynCapacity = 0;
    if ( m_dynamicChunk )
    {
      delete [] m_dynamicChunk;
      m_dynamicChunk = 0;
    }
  }
  unsigned int GetSize(){ return m_size; }
private:
  T   m_staticChunk[STATIC_ELEMENTS];
  T*  m_dynamicChunk;
  unsigned int m_dynCapacity;
  unsigned int m_size;
};

//===---------------------------------------------------------===//
// Simple stack based on the Static/Dynamic array above.
//===---------------------------------------------------------===//
template<typename T, int STATIC_ELEMENTS, int DYNAMIC_ELEMENTS_GRANULARITY>
class ScvalStaticDynStack
{
public:
  void Push( T h )
  {
    m_stack.Create() = h;
  }
  void Pop()
  {
    m_stack.RemoveBack();
  }
  T Top()
  {
    const unsigned int size = m_stack.GetSize();
    return size>0 ? m_stack.Get(size-1) : 0;
  }
  unsigned int GetSize(){ return m_stack.GetSize(); }
  void Clear(){ m_stack.Clear(); }
private:
  ScvalStaticDynArray<T,STATIC_ELEMENTS, DYNAMIC_ELEMENTS_GRANULARITY> m_stack;
};

//===---------------------------------------------------------===//
// Set. Very slow implementation using an array and linear search
// for each insert operation! Only used while compiling.
//===---------------------------------------------------------===//
template<typename T, int STATIC_ELEMENTS=128>
class ScvalSet
{
public:
  // returns the index where val was inserted
  unsigned int Set( unsigned int key, const T& val )
  {
    for ( unsigned int i = 0; i<m_list.GetSize(); ++i )
      if ( m_list.Get(i) == key )
        return i;
    m_list.Create() = val;
    return m_list.GetSize()-1;
  }
  // get the object given the index (not the key!)
  T& Get( unsigned int index )
  {
    return m_list.Get(index);
  }
  void Clear()
  {
    m_list.Clear();
  }
  unsigned int GetSize(){ return m_list.GetSize(); }
private:
  ScvalStaticDynArray<T,STATIC_ELEMENTS,STATIC_ELEMENTS> m_list;
};

//===---------------------------------------------------------===//
// Some type definitions and common values
//===---------------------------------------------------------===//
typedef unsigned short ScvalHandle;
typedef unsigned int ScvalHashID;
typedef unsigned short ScvalASTNodeType;
enum
{
  ROOTHANDLE=0,
  INVALIDHANDLE=0xffff,
  INVALIDHASH=0
};

//===---------------------------------------------------------===//
// Some forward declarations
//===---------------------------------------------------------===//
class ScvalParser;
struct ScvalVMCode;
struct ScvalASTGenCodeData;

//===---------------------------------------------------------===//
// This is an instruction in the VM. It's 32 bits.
// 1 byte for instruction type
// - for branches instructions, the address is 24 bits
// - for instructions with constant data segment, the address is 16 bits
// - for counter and string registers, it is a byte each
//===---------------------------------------------------------===//
struct ScvalVMOperation
{
  ScvalVMOperation& Set( unsigned char opc, unsigned char o0=0, unsigned char o1=0, unsigned char o2=0)
  {
    opcode = opc; op0 = o0; op1 = o1; op2 = o2;
    return *this;
  }
  void SetAddr( unsigned int addr )
  {
    op0 = unsigned char( (addr&0xff0000)>>16 );
    op1 = unsigned char( (addr&0x00ff00)>>8 );
    op2 = unsigned char(  addr&0x0000ff );
  }
  void SetDataAddr( unsigned int addr )
  {
    op1 = unsigned char( (addr&0xff00)>>8 );
    op2 = unsigned char(  addr&0x00ff );
  }
  unsigned int GetAddr()const { return unsigned int( (op0<<16) | (op1<<8) | (op2) ); }
  unsigned int GetDataAddr()const{ return unsigned int( (op1<<8) | (op2) ); }

  unsigned char opcode;
  unsigned char op0;
  unsigned char op1;
  unsigned char op2;
};

//===---------------------------------------------------------===//
// Node types in the AST (Abstract Syntax Tree)
//===---------------------------------------------------------===//
enum ScvalASTNodeEnum
{
  AST_ROOT=0,
  AST_ID,
  AST_REAL, AST_STR, AST_INT, AST_BOOL,
  AST_ONE, AST_ZERO_ONE, AST_ZERO_MORE, AST_ONE_MORE,
  AST_OR, AST_AND, AST_CHILDREN, AST_TYPEDEF, AST_ATTRS, AST_CALLBACK,
};

//===---------------------------------------------------------===//
// This is a node in the AST (32 bits)
//===---------------------------------------------------------===//
struct ScvalASTNode
{
  ScvalASTNodeType  type;       
  ScvalHandle       leaf;       // != INVALIDHANDLE when refers to a leaf
  ScvalHandle       firstchild; // first child index
  ScvalHandle       sibling;    // next sibling in my level
};
//===---------------------------------------------------------===//
// This ia a leaf node in the AST. A leaf is just a hash number
// representing a string. But in Debug, it is also the string and it's
// length (non zero terminated, caution).
//===---------------------------------------------------------===//
struct ScvalASTLeaf
{
  ScvalHashID id;
  bool operator ==(ScvalHashID hid ){ return id==hid; }
#ifdef _DEBUG
  const char* idname;
  unsigned short idlen;
#endif
};

//===---------------------------------------------------------===//
// Represents the Abstract Syntax Tree (AST) intermediate representation
// after parsing. It also has the code generation stuff for the tree.
//===---------------------------------------------------------===//
class ScvalAST
{
public:
  ScvalHandle PushNode( ScvalASTNodeType type );
  void PopNode();
  void InsertLeaf( ScvalASTNodeType leafType, const char* idname, unsigned short idlen );
  void Clear();
  ScvalASTNode& GetNode( ScvalHandle hNode );
  ScvalASTLeaf& GetLeaf( ScvalHandle hLeaf );
  bool IsEmpty(){ return m_nodes.GetSize()==0 && m_leaves.GetSize() == 0; }  
  bool GenerateCode(ScvalVMCode& code);
private:
  ScvalHandle AddNode( ScvalASTNodeType type );
  ScvalHandle AddLeaf( const char* idname, unsigned short idlen );  
  void AddChildNode( ScvalHandle hParent, ScvalHandle hChild );

  bool GenCodeChildrenElements( ScvalASTGenCodeData& code, const ScvalASTNode& node, int rbc, int rbs );
  bool GenCodeChildrenAttributes( ScvalASTGenCodeData& code, const ScvalASTNode& node, int rbc, int rbs );
  bool GenCodeChildElement( ScvalASTGenCodeData& code, const ScvalASTNode& node, int rbc, int rbs );
  bool GenCodeChildAttribute( ScvalASTGenCodeData& code, const ScvalASTNode& node, int rbc, int rbs );
  bool GenCodeCheckType( ScvalASTGenCodeData& code, const ScvalASTNode& node, int rbs );
  bool GenCodeCountersComparison( ScvalASTGenCodeData& code, const ScvalASTNode& node, int rbc );
private:
  friend class ScvalParser;
  ScvalStaticDynArray<ScvalASTNode,320,64>  m_nodes;
  ScvalSet<ScvalASTLeaf> m_leaves;
  ScvalStaticDynStack<ScvalHandle,32,8>  m_stack;
};

//===---------------------------------------------------------===//
// VM instructions set
//===---------------------------------------------------------===//
enum ScvalVMOpcode
{
  VM_LDEN, VM_LDEV,             // LoaD Element Name, LoaD Element Value
  VM_LDAN, VM_LDAV,             // LoaD Attribute Name, LoaD Attribute Value
  VM_CMPS, VM_CMPI,             // CoMPare String, CoMPare Integer
  VM_JE, VM_JNE, VM_JG, VM_JMP, // Jump instructions
  VM_CLR, VM_INC,               // CLeaR integer register, INCrement integer register
  VM_CHKN, VM_CHKC,             // CHeK Native type, CHeK Custom type
  VM_DOWN, VM_UP,               // go DOWN the xml tree, go UP the xml tree
  VM_GATT, VM_NATT,             // Go to ATTributes, Next ATTribute
  VM_NEXT, VM_RET,              // NEXT element, RETurn from subroutine
  VM_CALL,                      // CALLback

  VM_NILDATA=0xffff,            // Represents a NULL for data segment comparisons
  VM_ERRADDR=0xffffff           // Represents the error address to jump when we find an error
};

//===---------------------------------------------------------===//
// A context for the VM contains the:
// - Counter registers (short)
// - String Hashes registers (string comparisons)
// - String registers (type check validation)
//===---------------------------------------------------------===//
struct ScvalVMContext
{
  ScvalVMContext():m_regCounters(0),m_regStrHashes(0)
    ,m_regStrings(0),m_cmpRes(0),m_stringCount(0), m_checkStrReg(0){}

  void Clear();
  void Init( int regC, int regS );
  unsigned short* m_regCounters;
  ScvalHashID* m_regStrHashes;
  const char** m_regStrings;
  int m_cmpRes;
  int m_stringCount;
  int m_checkStrReg; // used as argument register when calling to check type subroutines
};

//===---------------------------------------------------------===//
// The bytecode. It contains the code and data segment.
//===---------------------------------------------------------===//
struct ScvalVMCode
{
  ScvalVMCode();
  ~ScvalVMCode(){Clear();}

  void Clear();
  unsigned int m_maxRegCounter; // max counter register used by the code
  unsigned int m_maxRegStrings; // max str register used by the code
  ScvalVMOperation* m_code;     // code segment
  unsigned int m_noOperations;
  ScvalHashID* m_constData;     // data segment
  unsigned int m_noConstData;  
};
//===---------------------------------------------------------===//
// Callback called during the bytecode execution for the actual
// XML read. It provides an abstract way for the user code to use
// whichever xml library he prefers.
//===---------------------------------------------------------===//
class ScvalInstHook
{
public:
  virtual const char* Do( ScvalVMOpcode opcode, ScvalHashID typeName=INVALIDHASH, const char* value=0 ) = 0;
};
class ScvalVM
{
public:
  ~ScvalVM(){Clear();}
  void Clear();
  bool Run( const ScvalVMCode* code, ScvalInstHook* hook );
private:
  bool IsInteger( const char* str );
  bool IsReal( const char* str );
  bool IsBool( ScvalHashID strhash );
private:
  unsigned int m_pc;
  unsigned int m_lastPc;
  unsigned int m_opExecuted;
  ScvalVMContext m_mainCtx;
};

//===---------------------------------------------------------===//
// Public common functions
//===---------------------------------------------------------===//
// Generates a hash from a string using the internal hash function
ScvalHashID ScvalHash(const char* str);

// Generates the bytecode from the text program
bool ScvalCompile(const char* text, ScvalVMCode& outBytecode );
void ScvalPrintCode( ScvalVMCode& code );

// Load the bytecode from binary chunk
bool ScvalLoadFromBinary( const void* binChunk, ScvalVMCode& outBytecode );

// Save the bytecode to binary chunk
bool ScvalSaveToBinary( const ScvalVMCode& inBytecode, void** outBinChunk, unsigned int& chunkSizeBytes );
void ScvalBinaryDeallocate( void** binChunk );

// Validates the XML from the bytecode
bool ScvalValidate(const ScvalVMCode& inBytecode, ScvalInstHook* xmlReader );

#endif