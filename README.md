# scval
C++ XML validator using a wildcards based custom language and a virtual machine for interpreting. 

A callback for actual XML parsing can be provided so this schema can be used in with any XML parsing lib.

# WIP
It's a Work in Progress, so you'd expect bugs, poor documentation and unconsistent coding style.

# Build
Visual Studio 2010 solution is provided.

# Sample
See main.cpp, provides a sample how to initialize, compile and run the validator, against a xml sample parsed with tinyXML2.

This is an example of the validator program for the included books.xml sample.
<pre>
 @author #AUTHOR
  @date #DATE
  @price #PRICE
  !catalog
  {
    *book[id(str)]
    {
        !author(author)
        !title(str)
        !genre(str)
        !price(price)
        !publish_date(date)
        !description(str)
    }
  }
 </pre>
 Which translates as:
* There's an only root node "catalog"
* With a variable number of sub nodes "book" (0..*)
* Each <book> contains a mandatory attribute <b>id</b> type of string.
  * and each book has mandatory nodes "author", "title", "genre"... each one of specified type with no attributes.

Note that special types as @author, @date and @price are sent back to c++ callback for actual data verification.
 
# How it works
 When you call to <i>ScvalCompile</i>, it compiles and generates the bytecode for the validator program.<br/>
 You can save/load this binary bytecode with  <i>ScvalLoadFromBinary/ScvalSaveToBinary</i>.<br/>
 Finally you can run the validator program by passing it to <i>ScvalValidate</i> which also receives a callback to return the actual XML data as attributes or nodes. That callback also will be in charge of validate specific strings, so more complex data validation can be performed in C++ for strings.<br/>
