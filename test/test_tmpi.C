/// \file
/// \Example macro to use TMPIFile.cxx
/// \This macro shows the usage of TMPIFile to simulate event reconstruction
///  and merging them in parallel
/// \To run this macro, once compiled, execute "mpirun -np <number of
/// processors> ./bin/test_tmpi" \macro_code \Author Amit Bashyal

#include "JetEvent.h"
#include "TError.h"
#include "TFile.h"
#include "TH1D.h"
#include "TMPIFile.h"
#include "TMemFile.h"
#include "TROOT.h"
#include "TRandom.h"
#include "TSystem.h"
#include "TTree.h"

#include "mpi.h"

#include <chrono>
#include <iostream>
#include <random>
#include <string>
#include <sstream>
#include <sys/types.h>
#include <thread>
#include <unistd.h>

/* ---------------------------------------------------------------------------

The idea of TMPIFile is to run N MPI ranks where some ranks are
producing data (called workers), while other ranks are collecting data and
writing it to disk (called collectors). The number of collectors can be  
configured and this should be optimized for each workflow and data size.

This example uses a typical event processing loop, where every N events the
TMPIFile::Sync() function is called. This call triggers the local TTree data
to be sent via MPI to the collector rank where it is merged with all the 
other worker rank data and written to a TFile.

An MPI Sub-Communictor is created for each collector which equally distributes
the remaining ranks to distribute the workers among collectors.

--------------------------------------------------------------------------- */

void test_tmpi() {
  
   Int_t N_collectors    = 2;     // specify how many collectors to run
   Int_t sync_rate       = 2;    // workers sync every sync_rate events
   Int_t events_per_rank = 6;    // total events each rank will produce then exit
   Int_t sleep_mean      = 5;     // simulate compute time for event processing
   Int_t sleep_sigma     = 2;     // variation in compute time

   // using JetEvent generator to create a data structure
   // these parameters control this generator
   Int_t jetm            = 25;
   Int_t trackm          = 60;
   Int_t hitam           = 200;
   Int_t hitbm           = 100;

   std::string treename  = "test_tmpi";
   std::string branchname = "event";

   // set output filename
   std::stringstream smpifname;
   smpifname << "/tmp/merged_output_" << getpid() << ".root";

   // Create new TMPIFile, passing the filename, setting read/write permissions
   // and setting the number of collectors.
   // If MPI_INIT has not been called already, the constructor of TMPIFile
   // will call this.
   TMPIFile *newfile = new TMPIFile(smpifname.str().c_str(), "RECREATE", N_collectors);
   // set random number seed that is based on MPI rank
   // this avoids producing the same events in each MPI rank
   gRandom->SetSeed(gRandom->GetSeed() + newfile->GetMPIGlobalRank());

   // only print log messages in MPI Rank 0
   if (newfile->GetMPIGlobalRank() == 0) {
      std::cout << " running with parallel ranks:   "
                << newfile->GetMPIGlobalSize() << "\n";
      std::cout << " running with collecting ranks: " << N_collectors << "\n";
      std::cout << " running with working ranks:    "
                << (newfile->GetMPIGlobalSize() - N_collectors) << "\n";
      std::cout << " running with sync rate:        " << sync_rate << "\n";
      std::cout << " running with events per rank:  " << events_per_rank << "\n";
      std::cout << " running with sleep mean:       " << sleep_mean << "\n";
      std::cout << " running with sleep sigma:      " << sleep_sigma << "\n";
      std::cout << " running with seed:             " << gRandom->GetSeed()
                << "\n";
   }

   // print filename for each collector Rank
   if (newfile->IsCollector()) {
      std::cout << "[" << newfile->GetMPIGlobalRank()
                << "] root output filename: " << smpifname.str() << std::endl;
   }

   // This if statement splits the run-time functionality of
   // workers and collectors.
   if (newfile->IsCollector()) {
      // Run by collector ranks
      // This will run until all workers have exited
      newfile->RunCollector(); 
   }
   else {  
      // Run by worker ranks
      // these ranks generate data to be written to TMPIFile

      // create a TTree to store event data
      TTree *tree = new TTree(treename.c_str(), "Event example with Jets");
      // set the AutoFlush rate to be the same as the sync_rate
      // this synchronizes the TTree branch compression
      tree->SetAutoFlush(sync_rate);

      // Create our fake event data generator
      JetEvent *event = new JetEvent;

      // add our branch to the TTree
      tree->Branch(branchname.c_str(), "JetEvent", &event, 8000, 2);

      // monitor timing
      auto sync_start = std::chrono::high_resolution_clock::now();

      // generate the specified number of events
      for (int i = 0; i < events_per_rank; i++) {

         auto start = std::chrono::high_resolution_clock::now();
         // Generate one event
         event->Build(jetm, trackm, hitam, hitbm);

         auto evt_built = std::chrono::high_resolution_clock::now();
         double build_time = std::chrono::duration_cast<std::chrono::duration<double>>(evt_built - start).count();

         std::cout << "[" << newfile->GetMPIColor() << "] "
                   << "[" << newfile->GetMPILocalRank() << "] evt = " << i
                   << "; build_time = " << build_time << std::endl;

         // if our build time was significant, subtract that from the sleep time
         auto adjusted_sleep = (int)(sleep_mean - build_time);
         auto sleep = abs(gRandom->Gaus(adjusted_sleep, sleep_sigma));

         // simulate the time taken by more complicated event generation
         std::this_thread::sleep_for(std::chrono::seconds(int(sleep)));

         // Fill the tree
         tree->Fill();

         // every sync_rate events, call the TMPIFile::Sync() function
         // to trigger MPI collection of local data
         if ((i + 1) % sync_rate == 0) {
            // call TMPIFile::Sync()
            newfile->Sync();

            auto end = std::chrono::high_resolution_clock::now();
            double sync_time = std::chrono::duration_cast<std::chrono::duration<double>>(end - sync_start).count();
            std::cout << "[" << newfile->GetMPIColor() << "] "
                      << "[" << newfile->GetMPILocalRank()
                      << "] event collection time: " << sync_time << std::endl;
            sync_start = std::chrono::high_resolution_clock::now();
         }
      }

      // synchronize any left over events
      if (events_per_rank % sync_rate != 0) {
         newfile->Sync();
      }
   }

   
   // call Close on the file for clean exit.
   std::cout << "[" << newfile->GetMPIColor() << "] "
                    << "[" << newfile->GetMPILocalRank()
                    << "] closing file\n";
   newfile->Close();

   // open file and test contents
   if (newfile->GetMPILocalRank() == 0) {
      TString filename = newfile->GetMPIFilename();
      std::cout << "[" << newfile->GetMPIColor() << "] "
                    << "[" << newfile->GetMPILocalRank()
                    << "] opening file " << filename << std::endl;
      TFile file(filename.Data());
      if(file.IsOpen()){
         file.ls();
         TTree* tree = (TTree*)file.Get(treename.c_str());
         if(tree)
            tree->Print();
         
      std::cout << "[" << newfile->GetMPIColor() << "] "
                    << "[" << newfile->GetMPILocalRank()
                    << "] file should have " << (newfile->GetMPILocalSize() - 1) * events_per_rank
                    << " events and has " << tree->GetEntries() << std::endl;
   
      }
   }

}

#ifndef __CINT__
int main() {
   auto start = std::chrono::high_resolution_clock::now();

   test_tmpi();

   auto end = std::chrono::high_resolution_clock::now();
   double time =
      std::chrono::duration_cast<std::chrono::duration<double>>(end - start)
          .count();
   std::string msg = "Total elapsed time: ";
   msg += std::to_string(time);
   Info("test_tmpi", msg.c_str());

   msg = "exiting ";
   Info("test_tmpi",msg.c_str());

   return 0;
}
#endif
