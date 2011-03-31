
// Qt includes
#include <QCoreApplication>
#include <QTextStream>

// ctkDICOMCore includes
#include "ctkDICOMDatabase.h"

// STD includes
#include <iostream>
#include <cstdlib>

int ctkDICOMTest1(int argc, char * argv []) {
  
  QCoreApplication app(argc, argv);
  QTextStream out(stdout);
  try
  {
    ctkDICOMDatabase myCTK( argv[1] );
    out << "open db success\n";
    /// make sure it is empty and properly initialized
    if (! myCTK.initializeDatabase() ) {
       out << "ERROR: basic DB init failed";
       return EXIT_FAILURE;
    };
    /// insert some sample data
    if (! myCTK.initializeDatabase(argv[2]) ) {
       out << "ERROR: sample DB init failed";
       return EXIT_FAILURE;
    };
    myCTK.closeDatabase(); 
    }
  catch (std::exception e)
  {
    out << "ERROR: " << e.what();
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}

