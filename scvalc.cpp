#include "scvaltypes.h"
#include <string.h>
#include <stdio.h>
#include <locale>

//===---------------------------------------------------------------------------===//
// LEXER
//===---------------------------------------------------------------------------===//
enum ScvalTokenType
{
  TOK_ERR,
  TOK_REAL, TOK_STR, TOK_INT, TOK_BOOL,
  TOK_ONE, TOK_ZERO_ONE, TOK_ZERO_MORE, TOK_ONE_MORE, TOK_COMMA,
  TOK_O_B, TOK_C_B, TOK_O_P, TOK_C_P, TOK_O_S, TOK_C_S,
  TOK_OR, TOK_TYPEDEF, TOK_ID, TOK_CALLBACK, TOK_CSTR,
  TOK_EOF
};
struct ScvalToken
{
  unsigned int offset;
  unsigned short len;
  unsigned short token;

  const char* GetText(const char* text){ return text+offset; }
};
struct ScvalLexer
{
  void Init(const char* text)
  { 
    m_text=m_cursor=m_lastCursor=text;
  }
  unsigned short NextToken(ScvalToken* t)
  {
    SkipBlanks();
    if ( IsEof() ) return SaveTokenAndReturn(t,TOK_EOF);
    m_lastCursor = m_cursor;
    const char nextChar = *m_cursor;
    switch ( nextChar )
    {
    case '@': ++m_cursor; return SaveTokenAndReturn(t,TOK_TYPEDEF);
    case '{': ++m_cursor; return SaveTokenAndReturn(t,TOK_O_B);
    case '}': ++m_cursor; return SaveTokenAndReturn(t,TOK_C_B);
    case '[': ++m_cursor; return SaveTokenAndReturn(t,TOK_O_S);
    case ']': ++m_cursor; return SaveTokenAndReturn(t,TOK_C_S);
    case '(': ++m_cursor; return SaveTokenAndReturn(t,TOK_O_P);
    case ')': ++m_cursor; return SaveTokenAndReturn(t,TOK_C_P);
    case '!': ++m_cursor; return SaveTokenAndReturn(t,TOK_ONE);
    case '|': ++m_cursor; return SaveTokenAndReturn(t,TOK_OR);
    case '?': ++m_cursor; return SaveTokenAndReturn(t,TOK_ZERO_ONE);
    case '*': ++m_cursor; return SaveTokenAndReturn(t,TOK_ZERO_MORE);
    case '+': ++m_cursor; return SaveTokenAndReturn(t,TOK_ONE_MORE);
    case '#': ++m_cursor; return SaveTokenAndReturn(t,TOK_CALLBACK);
    case '\'':
      ++m_cursor;
      m_lastCursor = m_cursor;
      while ( !IsEof() && *m_cursor!='\'' ) ++m_cursor;
      if ( !IsEof() )
      {
        SaveTokenAndReturn(t,TOK_CSTR);
        ++m_cursor;
        return TOK_CSTR;
      }
      break;
    default:
      if ( isalpha(nextChar) )
      {
        while ( !IsEof() && !IsBlank() && IsIdChar(*m_cursor) ) 
          ++m_cursor;
        return SaveTokenAndReturn(t,ExtractKeywordOrId());
      }
    }
    return SaveTokenAndReturn(t,TOK_ERR);
  }  
  bool IsEndXmlComment(const char* text)
  {
    return *text=='-' && *(text+1) && *(text+1)=='-' && 
            *(text+2) && *(text+2)=='>';
  }
  bool IsIdChar(const char c)
  {
    return isalnum(c)||c=='_';
  }
  unsigned short SaveTokenAndReturn(ScvalToken* t, ScvalTokenType tt )
  {
    t->token = tt;
    t->offset = unsigned int(m_lastCursor-m_text);
    t->len = unsigned short(m_cursor - m_lastCursor);
    return tt;
  }
  ScvalTokenType ExtractKeywordOrId()
  {
    int n = int(m_cursor-m_lastCursor);
    switch (n)
    {
    case 3: 
      if ( strncmp(m_lastCursor, "int", 3) == 0 ) return TOK_INT;
      if ( strncmp(m_lastCursor, "str", 3) == 0 ) return TOK_STR;
    break;
    case 4:
      if ( strncmp(m_lastCursor, "bool", 4) == 0 ) return TOK_BOOL;
      if ( strncmp(m_lastCursor, "real", 4) == 0 ) return TOK_REAL;
    break;
    }
    return TOK_ID;
  }
  inline bool IsBlank(){char c=*m_cursor; return c==' '||c=='\t'||c=='\n'||c=='\r';}
  inline bool IsEof(){return *m_cursor==0; }
  inline void SkipBlanks(){ while ( !IsEof() && IsBlank() ) ++m_cursor; }

  const char* m_text;
  const char* m_cursor;
  const char* m_lastCursor;
};

//===---------------------------------------------------------------------------===//
// AST
//===---------------------------------------------------------------------------===//
#ifdef _DEBUG
ScvalAST* g_ast=NULL;
char g_tmp[64];
const char* astnames[]=
{"root", "id", "real", "str", "int", "bool", "one", "zero_one", 
"zero_more", "one_more", "or", "and", "children", "typedef", 
"attrs", "callback" };
void ScvalPrintAST( const ScvalASTNode& node, int level=0 )
{
  for(int i=0;i<level;++i)printf("  ");
  if ( node.leaf !=INVALIDHANDLE )
  {
    const ScvalASTLeaf& l = g_ast->GetLeaf( node.leaf );    
    sprintf_s(g_tmp,"%%.%ds\n", l.idlen );
    printf( g_tmp, l.idname );
    return;
  }
  printf( "[%s]\n", astnames[node.type]);
  ScvalHandle h=node.firstchild;
  while ( h != INVALIDHANDLE )
  {
    const ScvalASTNode& n = g_ast->GetNode(h);
    ScvalPrintAST( n, level+1 );
    h = n.sibling;
  }  
}
#endif
static inline int HashN(const char *sym,int n)
{
  register int hash = 5381;
  register int i=0;
  while (*sym && i<n)
  {
    hash = ((hash << 5) + hash) ^ *(sym++);
    ++i;
  }
  return hash;
} // hash_string_djbxor
static inline int Hash(const char *sym)
{
  if ( !sym ) return 0;
  register int hash = 5381;
  while (*sym)
    hash = ((hash << 5) + hash) ^ *(sym++);
  return hash;
} // hash_string_djbxor

ScvalHashID ScvalHash(const char* str)
{
  return (ScvalHashID)Hash(str);
}
void ScvalAST::AddChildNode( ScvalHandle hParent, ScvalHandle hChild )
{
  if ( hParent == INVALIDHANDLE )
    return;

  ScvalASTNode& nParent = GetNode(hParent);
  if ( nParent.firstchild == INVALIDHANDLE )
  {
    // no children, set first one
    nParent.firstchild = hChild;
  }else
  {
    // traverse siblings until next one (rightmost node)
    ScvalHandle next=nParent.firstchild, prev=nParent.firstchild;
    while ( next != INVALIDHANDLE )
    {
      prev = next;
      next = GetNode(next).sibling;
    }
    GetNode(prev).sibling = hChild;
  }
}

ScvalHandle ScvalAST::PushNode( ScvalASTNodeType type )
{
  ScvalHandle hTop = m_stack.GetSize()>0?m_stack.Top():INVALIDHANDLE;
  ScvalHandle hNode= AddNode( type );
  ScvalASTNode&  nNode= GetNode(hNode);
  m_stack.Push( hNode );
  if ( hTop != INVALIDHANDLE )
    AddChildNode( hTop, hNode );
  return hNode;
}
void ScvalAST::PopNode()
{
  m_stack.Pop();
}
void ScvalAST::InsertLeaf( ScvalASTNodeType leafType, const char* idname, unsigned short idlen )
{
  ScvalHandle hLeafNode = PushNode(leafType);
  GetNode(hLeafNode).leaf = AddLeaf(idname,idlen);
  PopNode();
}
ScvalHandle ScvalAST::AddNode( ScvalASTNodeType type )
{
  ScvalASTNode& node = m_nodes.Create();
  node.firstchild = node.leaf = node.sibling = INVALIDHANDLE;
  node.type = type;
  return m_nodes.GetSize()-1;
}
ScvalHandle ScvalAST::AddLeaf( const char* idname, unsigned short idlen )
{
  ScvalHashID hashId = HashN(idname,idlen);
  ScvalASTLeaf leaf;
  leaf.id = hashId;
#ifdef _DEBUG
  leaf.idname = idname;
  leaf.idlen = idlen;
#endif
  ScvalHandle hLeaf = m_leaves.Set( hashId, leaf );  
  return hLeaf;
}
ScvalASTNode& ScvalAST::GetNode( ScvalHandle hNode )
{
  return m_nodes.Get(hNode);
}
ScvalASTLeaf& ScvalAST::GetLeaf( ScvalHandle hLeaf )
{
  return m_leaves.Get(hLeaf);
}
void ScvalAST::Clear()
{
  m_nodes.Clear();
  m_leaves.Clear();
  m_stack.Clear();
}
//===---------------------------------------------------------------------------===//
//===---------------------------------------------------------------------------===//
struct NodeScoper
{
  ScvalAST& m_ast;
  NodeScoper(ScvalAST& ast, ScvalASTNodeType t):m_ast(ast){ m_ast.PushNode(t);}
  ~NodeScoper(){m_ast.PopNode();}
};
#define NODESCOPE(rc) NodeScoper _scope##rc(m_ast,rc)

//===---------------------------------------------------------------------------===//
// PARSER
//===---------------------------------------------------------------------------===//
class ScvalParser
{
public:
  ScvalParser(){}
  bool Parse( const char* text);
  bool GenerateCode(ScvalVMCode& outByteCode);

private:
  bool ParseTypedef();
  bool ParseTypedefBody();
  bool ParseTypedefExpr();
  bool ParseTypedefEnum();
  bool ParseTypedefList();
  bool ParseElementDef();
  bool ParseElement();
  bool ParseAttributeList();
  bool ParseAttributeDef();
  bool ParseAttribute();
  bool ParseType();

private:
  ScvalAST m_ast;
};
struct ScvalASTGenCodeData
{
  unsigned int m_maxRegCounter;
  unsigned int m_maxRegStrings;
  ScvalStaticDynArray<ScvalVMOperation,256,256> m_code;
  ScvalSet<ScvalHashID,64> m_constData;
};

#define CONSUME() g_lexer.NextToken(&g_token)
#define EXPECTEDNC(tok) { if ( g_token.token != tok ) return false; }
#define EXPECTED(tok) { if ( g_token.token != tok ) return false; else CONSUME(); }
#define LEAF(id) m_ast.InsertLeaf( id, g_token.GetText(g_lexer.m_text), g_token.len )
#define EXPECTEDLEAF(tok,leaf) EXPECTEDNC(tok); LEAF(leaf); CONSUME()
ScvalTokenType g_tokenEof=TOK_EOF;
ScvalToken g_token;
ScvalLexer g_lexer;
bool ScvalParser::Parse( const char* text )
{
  if ( !text || !*text ) return false;

  g_lexer.Init(text);
  m_ast.Clear();
  NODESCOPE(AST_ROOT);
  CONSUME();
  bool validSyntax=true;
  while ( validSyntax && g_token.token != g_tokenEof )
  {
    validSyntax=false;
    switch ( g_token.token )
    {
      case TOK_TYPEDEF  : CONSUME(); validSyntax = ParseTypedef(); break;
      default: { NODESCOPE(AST_CHILDREN); validSyntax = ParseElementDef(); }break;
    }    
  }    
#ifdef _DEBUG
  g_ast = &m_ast;
  ScvalPrintAST(m_ast.GetNode(ROOTHANDLE),0);
#endif
  return true;  
}
bool ScvalParser::GenerateCode( ScvalVMCode& valCode )
{
  valCode.Clear();
  if ( m_ast.IsEmpty() ) 
    return false;
  if ( !m_ast.GenerateCode(valCode) )
    return false;  
  return true;
}
bool ScvalParser::ParseTypedef()
{
  NODESCOPE(AST_TYPEDEF);
  EXPECTEDLEAF(TOK_ID, AST_ID);
  if ( ! ParseTypedefBody() )
    return false;
  return true;
}
bool ScvalParser::ParseTypedefBody()
{
  if ( ParseTypedefExpr() )
    return true;
  if ( g_token.token == TOK_CALLBACK )
  {
    NODESCOPE(AST_CALLBACK);
    CONSUME();
    EXPECTEDLEAF(TOK_ID,AST_ID);    
    return true;
  }
  return false;
}
bool ScvalParser::ParseTypedefExpr()
{
  switch ( g_token.token )
  {
  case TOK_O_P: { CONSUME(); NODESCOPE(AST_OR); return ParseTypedefEnum(); }
  case TOK_O_S: { CONSUME(); NODESCOPE(AST_AND); return ParseTypedefList(); }
  case TOK_REAL: LEAF( AST_REAL); CONSUME(); return true;
  case TOK_INT:  LEAF( AST_INT ); CONSUME(); return true;
  case TOK_STR:  LEAF( AST_STR ); CONSUME(); return true;
  case TOK_CSTR: LEAF( AST_ID  ); CONSUME(); return true;
  case TOK_BOOL: LEAF( AST_BOOL); CONSUME(); return true;
  case TOK_ID:   LEAF( AST_ID  ); CONSUME(); return true;
  }
  return false;
}
bool ScvalParser::ParseTypedefEnum()
{
  if ( !ParseTypedefExpr() ) 
    return false;
  if ( g_token.token == TOK_OR )
  {
    CONSUME();
    return ParseTypedefEnum();
  }
  EXPECTED(TOK_C_P);
  return true;
}
bool ScvalParser::ParseTypedefList()
{  
  while ( ParseTypedefExpr() ) ;
  EXPECTED(TOK_C_S);
  return true;
}
bool ScvalParser::ParseElementDef()
{
  switch ( g_token.token )
  {
  case TOK_ONE      : { NODESCOPE(AST_ONE); CONSUME(); return ParseElement(); }
  case TOK_ZERO_ONE : { NODESCOPE(AST_ZERO_ONE); CONSUME(); return ParseElement(); }
  case TOK_ZERO_MORE: { NODESCOPE(AST_ZERO_MORE); CONSUME(); return ParseElement(); }
  case TOK_ONE_MORE : { NODESCOPE(AST_ONE_MORE); CONSUME(); return ParseElement(); }
  }
  return false;
}
bool ScvalParser::ParseElement()
{
  EXPECTEDLEAF(TOK_ID,AST_ID);
  // element type optional
  if ( g_token.token == TOK_O_P )
  {
    CONSUME();
    if ( !ParseType() )
      return false;
    EXPECTED(TOK_C_P);
  }
  // element attributes optional
  if ( g_token.token == TOK_O_S )
  { 
    CONSUME(); 
    NODESCOPE(AST_ATTRS);
    if ( !ParseAttributeList() )
      return false;    
  }
  // element children
  if ( g_token.token == TOK_O_B )
  {
    CONSUME();
    NODESCOPE(AST_CHILDREN);
    while ( ParseElementDef() );
    EXPECTED(TOK_C_B);
  }  
  return true;
}
bool ScvalParser::ParseAttributeList()
{
  while ( ParseAttributeDef() ) ;
  EXPECTED(TOK_C_S);
  return true;
}
bool ScvalParser::ParseAttributeDef()
{
  switch ( g_token.token )
  {
  case TOK_ID       : { NODESCOPE(AST_ONE); return ParseAttribute(); }
  case TOK_ONE      : { NODESCOPE(AST_ONE);       CONSUME(); return ParseAttribute(); }
  case TOK_ZERO_ONE : { NODESCOPE(AST_ZERO_ONE);  CONSUME(); return ParseAttribute(); }
  case TOK_ZERO_MORE: { NODESCOPE(AST_ZERO_MORE); CONSUME(); return ParseAttribute(); }
  case TOK_ONE_MORE : { NODESCOPE(AST_ONE_MORE);  CONSUME(); return ParseAttribute(); }
  }
  return false;
}
bool ScvalParser::ParseType()
{
  switch ( g_token.token )
  {
  case TOK_ID   : LEAF(AST_ID);   CONSUME(); return true;
  case TOK_REAL : LEAF(AST_REAL); CONSUME(); return true;
  case TOK_STR  : LEAF(AST_STR);  CONSUME(); return true;
  case TOK_BOOL : LEAF(AST_BOOL); CONSUME(); return true;
  case TOK_INT  : LEAF(AST_INT);  CONSUME(); return true;
  }
  return false;
}
bool ScvalParser::ParseAttribute()
{
  EXPECTEDLEAF(TOK_ID,AST_ID);
  EXPECTED(TOK_O_P);
  if ( ! ParseType() )
    return false;
  EXPECTED(TOK_C_P);
  return true;
}

//===---------------------------------------------------------------------------===//
// CODE GENERATION
//===---------------------------------------------------------------------------===//
void ScvalPrintCode( ScvalVMCode& code )
{
#if _DEBUG
  const char* opnames[]={
    "lden", "ldev", "ldan", "ldav", "cmps", "cmpi", 
    "je  ", "jne ", "jg  ", "jmp ", "clr ", "inc ", 
    "chkn", "chkc",
    "down", "up  ", "gatt", "natt", "next",
    "ret ", "call"};
    const int opcount[]={ 
      1, 1, 1, 1, 3, 2,
      3, 3, 3, 3, 0, 1, 
      2, 3, 
      0, 0, 0, 0, 0, 
      0, 3 };
      for ( unsigned int i = 0; i < code.m_noOperations; ++i )
      {
        ScvalVMOperation& op = code.m_code[i];
        printf( "%03d %s ", i, opnames[op.opcode] );
        if ( op.GetAddr() == VM_ERRADDR )
          printf( "err" );
        else if ( op.GetDataAddr() == VM_NILDATA )
          printf( "nil" );
        else
          for ( int j=0; j< opcount[op.opcode]; ++j )
            printf( "%d ", *((&op.op0)+j) );
        printf( "\n" );
      }
      for ( unsigned int i = 0; i < code.m_noConstData; ++i )
        printf( "[%02i] 0x%08x\n", i, code.m_constData[i] );
      printf( "\nNo. CRegs=%d\n", code.m_maxRegCounter+1);
      printf( "No. SRegs=%d\n", code.m_maxRegStrings+1);
      printf( "Data segment=%d\n", code.m_noConstData );  
      printf( "Program size=%d bytes\n", code.m_noOperations*sizeof(ScvalVMOperation) + code.m_noConstData*sizeof(ScvalHashID) );
#endif
}
bool ScvalAST::GenerateCode(ScvalVMCode& code)
{
  // main code
  unsigned int lastOp=0;
  ScvalASTGenCodeData genCode;
  ScvalASTNode& root = GetNode(ROOTHANDLE);
  ScvalHandle h=root.firstchild;
  while ( h != INVALIDHANDLE )
  {
    ScvalASTNode& n = GetNode(h);
    if ( n.type == AST_CHILDREN )
    {
      if ( !GenCodeChildrenElements(genCode,n,0,0) )
        return false;
      lastOp = genCode.m_code.GetSize();
      genCode.m_code.Create().Set( VM_JMP );
    }
    h = n.sibling;
  }

  // Subroutines for type checking
  h=root.firstchild;
  while ( h != INVALIDHANDLE )
  {
    ScvalASTNode& n = GetNode(h);
    if ( n.type == AST_TYPEDEF )
    {
      ScvalASTNode& nFirst = GetNode(n.firstchild);
      // search for call to this type in the whole code, and substitute with correct addr
      for ( unsigned int i = 0; i < genCode.m_code.GetSize(); ++i )
      {
        ScvalVMOperation& op = genCode.m_code.Get(i);
        if ( op.opcode == VM_CHKC )
        {
          ScvalHandle lh = nFirst.leaf;
          if ( op.GetDataAddr() == lh )
            op.SetDataAddr( genCode.m_code.GetSize() );
        }
      }
      // generate code for this type check
      if ( nFirst.sibling != INVALIDHANDLE )
      {
        ScvalASTNode& nSecond = GetNode(nFirst.sibling);
        switch ( nSecond.type )
        {
        case AST_CALLBACK : 
          {
          ScvalASTNode& cbnode = GetNode( nSecond.firstchild );
          unsigned int dataAddr = genCode.m_constData.Set( cbnode.leaf, GetLeaf(cbnode.leaf).id );
          genCode.m_code.Create().Set( VM_CALL ).SetDataAddr(dataAddr);
          genCode.m_code.Create().Set( VM_JE ).SetAddr( VM_ERRADDR );
          }break;
        case AST_OR       : break;
        case AST_AND      : break;
        }
      }
      // back to main execution
      genCode.m_code.Create().Set(VM_RET);
    }
    h = n.sibling;
  }  

  // last main code operation should jump to the end of total code (right after type checking routines)
  genCode.m_code.Get(lastOp).SetAddr( genCode.m_code.GetSize() );

  // filling final code/data container
  code.Clear();
  if ( genCode.m_code.GetSize() > 0 )
  {
    code.m_noOperations = genCode.m_code.GetSize();
    code.m_code = (ScvalVMOperation*)malloc( sizeof(ScvalVMOperation)*code.m_noOperations );
    for ( unsigned int i=0; i < code.m_noOperations; ++i )
      code.m_code[i] = genCode.m_code.Get(i);
  }
  if ( genCode.m_constData.GetSize() > 0 )
  {
    code.m_noConstData = genCode.m_constData.GetSize();
    code.m_constData = (ScvalHashID*)malloc( sizeof(ScvalHashID)*code.m_noConstData );
    for ( unsigned int i=0; i < code.m_noConstData; ++i )
      code.m_constData[i] = genCode.m_constData.Get(i);
  }
  code.m_maxRegCounter = genCode.m_maxRegCounter;
  code.m_maxRegStrings = genCode.m_maxRegStrings;
  return true;
}
bool ScvalAST::GenCodeChildrenElements( ScvalASTGenCodeData& code, const ScvalASTNode& node, int rbc, int rbs )
{
  // while read element
  unsigned int whileAddr = code.m_code.GetSize();
  code.m_code.Create().Set( VM_LDEN, rbs ); 
  code.m_code.Create().Set( VM_CMPS, rbs, 0xff, 0xff ); // cmps with nil
  ScvalVMOperation& opJe = code.m_code.Create().Set( VM_JE );
  ScvalHandle h=node.firstchild;
  int rc=rbc;
  ScvalStaticDynArray<unsigned int,32,32> jmpToNextElm;
  while ( h != INVALIDHANDLE )
  {
    ScvalASTNode& n = GetNode(h);
    switch ( n.type )
    {
    case AST_ONE:
    case AST_ONE_MORE:
    case AST_ZERO_MORE:
    case AST_ZERO_ONE: 
      if ( ! GenCodeChildElement(code, n,rc++,rbs) ) 
        return false;
      jmpToNextElm.Create()=code.m_code.GetSize()-1;
      break;
    }
    h = n.sibling;
  }
  code.m_code.Create().Set(VM_JMP).SetAddr(VM_ERRADDR); // jmp err
  for ( unsigned int i = 0; i < jmpToNextElm.GetSize(); ++i )
    code.m_code.Get(jmpToNextElm.Get(i)).SetAddr( code.m_code.GetSize() );
  code.m_code.Create().Set(VM_NEXT);
  code.m_code.Create().Set(VM_JMP).SetAddr(whileAddr);
  if ( ! GenCodeCountersComparison(code, node, rbc) )
    return false;  
  opJe.SetAddr( code.m_code.GetSize() );

  if ( rbs > (int)code.m_maxRegStrings )
    code.m_maxRegStrings = rbs;  
  return true;
}
bool ScvalAST::GenCodeCountersComparison( ScvalASTGenCodeData& code, const ScvalASTNode& node, int rbc )
{
  ScvalHandle h=node.firstchild;
  int rc = rbc;
  while ( h != INVALIDHANDLE )
  {
    ScvalASTNode& n = GetNode(h);
    switch ( n.type )
    {
    case AST_ONE: 
      code.m_code.Create().Set( VM_CMPI, rc++, 1 );
      code.m_code.Create().Set( VM_JNE ).SetAddr(VM_ERRADDR); // jne err
      break;
    case AST_ONE_MORE: 
      code.m_code.Create().Set( VM_CMPI, rc++, 0 );
      code.m_code.Create().Set( VM_JE ).SetAddr(VM_ERRADDR);// je err
      break;
    case AST_ZERO_ONE: 
      code.m_code.Create().Set( VM_CMPI, rc++, 1 );
      code.m_code.Create().Set( VM_JG).SetAddr(VM_ERRADDR);// jg err
      break;
    }
    h = n.sibling;
  }
  if ( rc > (int)code.m_maxRegCounter )
    code.m_maxRegCounter = rc;  
  return true;
}
bool ScvalAST::GenCodeChildrenAttributes( ScvalASTGenCodeData& code, const ScvalASTNode& node, int rbc, int rbs )
{
  // while read attributes
  code.m_code.Create().Set(VM_GATT);
  unsigned int whileAddr=code.m_code.GetSize();
  code.m_code.Create().Set(VM_LDAN, rbs);
  code.m_code.Create().Set(VM_CMPS, rbs, 0xff, 0xff );
  ScvalVMOperation& opJe = code.m_code.Create().Set(VM_JE);
  ScvalHandle h=node.firstchild;
  int rc=rbc;
  ScvalStaticDynArray<unsigned int,32,32> jmpToNextAtt;
  while ( h != INVALIDHANDLE )
  {
    ScvalASTNode& n = GetNode(h);
    switch ( n.type )
    {
    case AST_ONE:
    case AST_ONE_MORE:
    case AST_ZERO_MORE:
    case AST_ZERO_ONE: 
      if ( ! GenCodeChildAttribute(code, n, rc++, rbs) ) 
        return false; 
      jmpToNextAtt.Create()=code.m_code.GetSize()-1;
      break;
    }
    h = n.sibling;
  }
  code.m_code.Create().Set(VM_JMP).SetAddr(VM_ERRADDR); // jmp err
  for ( unsigned int i = 0; i < jmpToNextAtt.GetSize(); ++i )
    code.m_code.Get(jmpToNextAtt.Get(i)).SetAddr( code.m_code.GetSize() );
  code.m_code.Create().Set( VM_NATT );
  code.m_code.Create().Set( VM_JMP ).SetAddr( whileAddr );
  if ( ! GenCodeCountersComparison(code, node,rbc) )
    return false;  
  opJe.SetAddr( code.m_code.GetSize() );
  if ( rbs > (int)code.m_maxRegStrings )
    code.m_maxRegStrings = rbs;
  return true;
}
bool ScvalAST::GenCodeChildAttribute( ScvalASTGenCodeData& code, const ScvalASTNode& node, int rbc, int rbs )
{
  ScvalASTNode& n = GetNode(node.firstchild);
  unsigned int dataAddr = code.m_constData.Set( n.leaf, GetLeaf(n.leaf).id );
  code.m_code.Create().Set( VM_CMPS, rbs ).SetDataAddr(dataAddr);
  ScvalVMOperation& opJne = code.m_code.Create().Set( VM_JNE );
  code.m_code.Create().Set( VM_INC, rbc );
  code.m_code.Create().Set( VM_LDAV, rbs+1 );
  if ( ! GenCodeCheckType(code, GetNode(n.sibling), rbs+1) )
    return false;
  //finish the inner body of the CMPS (when it's true), so jump to the end of if chain (like a switch)
  code.m_code.Create().Set( VM_JMP ); // jmp to natt, the addr will be filled in GenCodeChildrenElemen
  opJne.SetAddr( code.m_code.GetSize() );
  if ( rbc > (int)code.m_maxRegCounter )
    code.m_maxRegCounter = rbc;
  return true;
}
bool ScvalAST::GenCodeCheckType( ScvalASTGenCodeData& code, const ScvalASTNode& node, int rbs )
{
  switch ( node.type )
  {
  case AST_REAL: code.m_code.Create().Set( VM_CHKN, rbs, 0 ); break;
  case AST_STR : code.m_code.Create().Set( VM_CHKN, rbs, 1 ); break;
  case AST_INT : code.m_code.Create().Set( VM_CHKN, rbs, 2 ); break;
  case AST_BOOL: code.m_code.Create().Set( VM_CHKN, rbs, 3 ); break;
  case AST_ID  :
    {
      code.m_constData.Set( node.leaf, GetLeaf(node.leaf).id );
      code.m_code.Create().Set( VM_CHKC, rbs ).SetDataAddr(node.leaf);
    }break;
  }
  if ( rbs > (int)code.m_maxRegStrings )
    code.m_maxRegStrings = rbs;
  return true;
}
bool ScvalAST::GenCodeChildElement( ScvalASTGenCodeData& code, const ScvalASTNode& node, int rbc, int rbs )
{
  ScvalASTNode& n = GetNode(node.firstchild);
  unsigned int dataAddr = code.m_constData.Set( n.leaf, GetLeaf(n.leaf).id );
  code.m_code.Create().Set( VM_CMPS, rbs ).SetDataAddr( dataAddr );
  ScvalVMOperation& opJne = code.m_code.Create().Set( VM_JNE );
  code.m_code.Create().Set( VM_INC, rbc );
  ScvalHandle h = GetNode(node.firstchild).sibling;
  while ( h != INVALIDHANDLE )
  {
    ScvalASTNode& n = GetNode(h);
    switch ( n.type )
    {
    case AST_ATTRS: 
      if ( ! GenCodeChildrenAttributes(code, n, rbc+1, rbs+1) )
        return false;
      break;
    case AST_CHILDREN: 
      code.m_code.Create().Set( VM_DOWN );
      if ( ! GenCodeChildrenElements(code, n, rbc+1, rbs+1) )
        return false; 
      code.m_code.Create().Set( VM_UP );
      break;
    default:
      if ( n.leaf != INVALIDHANDLE )
      {
        code.m_code.Create().Set( VM_LDEV, rbs+1 );
        if ( !GenCodeCheckType(code, n, rbs+1) )
          return false;
      }
    }
    h = n.sibling;
  }
  //finish the inner body of the CMPS (when it's true), so jump to the end of if chain (like a switch)
  code.m_code.Create().Set( VM_JMP ); // jmp to next, the addr will be filled in GenCodeChildrenElements
  opJne.SetAddr( code.m_code.GetSize() );
  if ( rbc > (int)code.m_maxRegCounter )
    code.m_maxRegCounter = rbc;
  if ( rbs > (int)code.m_maxRegStrings )
    code.m_maxRegStrings = rbs;
  return true;
}
//===---------------------------------------------------------------------------===//
//===---------------------------------------------------------------------------===//
bool ScvalCompile(const char* text, ScvalVMCode& outBytecode )
{
  ScvalParser parser;
  return parser.Parse(text) && parser.GenerateCode(outBytecode);
}

#undef CONSUME
#undef EXPECTED
#undef LEAF
#undef EXPECTEDLEAF