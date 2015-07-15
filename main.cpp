#include <stdio.h>
#include "scvaltypes.h"
#include "tinyxml2/tinyxml2.h"
#include <stack>

// Provide callbacks for specific operations in the validator,
// based on TinyXML library.
class TinyXMLHooks : public ScvalInstHook
{
public:
  TinyXMLHooks(const char* xmlfile) : m_xmlElmt(0), m_xmlAttr(0)
  {
    if ( doc.LoadFile(xmlfile) == tinyxml2::XML_SUCCESS )
      m_xmlElmt = doc.FirstChildElement();
    else
      doc.PrintError();
  }
  virtual const char* Do( ScvalVMOpcode opcode, ScvalHashID typeName=INVALIDHASH, const char* value=0 )
  {
    switch ( opcode )
    {
    case VM_LDEN: 
      return m_xmlElmt ? m_xmlElmt->Name() : NULL;
    case VM_LDEV: 
      return m_xmlElmt ? m_xmlElmt->GetText() : NULL;
    case VM_LDAN: 
      return m_xmlAttr ? m_xmlAttr->Name() : NULL;
    case VM_LDAV:
      return m_xmlAttr ? m_xmlAttr->Value() : NULL;
    case VM_DOWN: 
      m_elmstack.push( m_xmlElmt );
      m_xmlElmt = m_xmlElmt->FirstChildElement(); 
      break;
    case VM_UP  : 
      m_xmlElmt = m_elmstack.top();
      m_elmstack.pop();
      break;
    case VM_GATT:
      m_xmlAttr = m_xmlElmt->FirstAttribute(); 
      break;
    case VM_NATT:
      m_xmlAttr = m_xmlAttr->Next(); 
      break;
    case VM_NEXT:
      m_xmlElmt = m_xmlElmt->NextSiblingElement(); 
      break;
    case VM_CALL:
        if ( typeName == ScvalHash("AUTHOR") )      return (const char*)CheckAuthor(value);
        else if ( typeName == ScvalHash("DATE") )   return (const char*)CheckDate(value);
        else if ( typeName == ScvalHash("PRICE") )  return (const char*)CheckPrice(value);
        return 0;
      break;
    }
    return NULL;
  }
protected:
  int CheckAuthor(const char* authorStr)
  {
    // might check a DB with Authors (for example)

    return 1;
  }
  int CheckDate(const char* dateStr)
  {
    // check syntax and semantic

    return 1;
  }
  int CheckPrice(const char* priceStr)
  {
    // syntax correct? price range correct?
    return 1;
  }
protected:
  tinyxml2::XMLDocument doc;
  tinyxml2::XMLElement* m_xmlElmt;
  const tinyxml2::XMLAttribute* m_xmlAttr;
  std::stack<tinyxml2::XMLElement*> m_elmstack;
};

void TestBooks()
{
  const char* schema ="\
  @author #AUTHOR\
  @date #DATE\
  @price #PRICE\
  !catalog\
  {\
    *book[id(str)]\
    {\
        !author(author)\
        !title(str)\
        !genre(str)\
        !price(price)\
        !publish_date(date)\
        !description(str)\
    }\
  }";
  printf( "AST\n====\n" );
  ScvalVMCode bytecode;
  if ( ! ScvalCompile( schema, bytecode ) )
  {
    printf( "Error building scval bytecode\n" );
  }
  printf( "BYTECODE\n========\n" );
  ScvalPrintCode(bytecode);

  // testing binary bytecode save...
  unsigned int binSize;
  void* bytecodeBin;
  ScvalSaveToBinary( bytecode, &bytecodeBin, binSize );
  
  // ...and load again
  bytecode.Clear();
  ScvalLoadFromBinary( bytecodeBin, bytecode );
  ScvalBinaryDeallocate( &bytecodeBin );

  // validating our xml file with the compiled validator
  // our hook provides XML reading by using TinyXML
  // and the proper custom data type check
  TinyXMLHooks xmlHook("books.xml");
  printf( "\nValidating xml...\n" );
  if ( ! ScvalValidate( bytecode, &xmlHook ) )
    printf( "Error, XML is not valid\n" );
  else
    printf( "OK\n" );
}

int main()
{
  TestBooks();
  return 0;
}