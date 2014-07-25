// -*- mode: C++; c-basic-offset: 3; -*-

#include <config.h>

#include <apt-pkg/algorithms.h>
#include <apt-pkg/aptconfiguration.h>
#include <apt-pkg/cachefile.h>
#include <apt-pkg/cacheset.h>
#include <apt-pkg/cmndline.h>
#include <apt-pkg/deblistparser.h>
#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/indexfile.h>
#include <apt-pkg/indexrecords.h>
#include <apt-pkg/init.h>
#include <apt-pkg/pkgrecords.h>
#include <apt-pkg/pkgsystem.h>
#include <apt-pkg/progress.h>
#include <apt-pkg/sourcelist.h>
#include <apt-pkg/srcrecords.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/version.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/macros.h>
#include <apt-pkg/pkgcache.h>
#include <apt-pkg/cacheiterators.h>

#include <apt-private/acqprogress.h>
#include <apt-private/private-cacheset.h>
#include <apt-private/private-cachefile.h>
#include <apt-private/private-install.h>
#include <apt-private/private-main.h>
#include <apt-private/private-output.h>

#include <errno.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/statvfs.h>
#include <sys/wait.h>
#include <unistd.h>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <vector>

#include <apti18n.h>

#define addArg(w,x,y,z) Args.push_back(CommandLine::MakeArgs(w,x,y,z))

using namespace std;


// TryToInstallBuildDep - Try to install a single package
// ---------------------------------------------------------------------
/* This used to be inlined in DoInstall, but with the advent of regex package
   name matching it was split out.. */
static bool TryToInstallBuildDep(pkgCache::PkgIterator Pkg,
                                 pkgCacheFile &Cache,
                                 pkgProblemResolver &Fix,
                                 bool Remove,
                                 bool BrokenFix,
                                 bool AllowFail = true)
{
   if (Cache[Pkg].CandidateVer == 0 && Pkg->ProvidesList != 0)
   {
      CacheSetHelperAPTGet helper(c1out);
      helper.showErrors(false);
      pkgCache::VerIterator Ver = helper.canNotFindNewestVer(Cache, Pkg);
      if (Ver.end() == false)
         Pkg = Ver.ParentPkg();
      else if (helper.showVirtualPackageErrors(Cache) == false)
         return AllowFail;
   }

   if (_config->FindB("Debug::BuildDeps", false) == true)
   {
      if (Remove == true)
         cerr << "  Trying to remove " << Pkg << endl;
      else
         cerr << "  Trying to install " << Pkg << endl;
   }

   if (Remove == true)
   {
      TryToRemove RemoveAction(Cache, &Fix);
      RemoveAction(Pkg.VersionList());
   } else if (Cache[Pkg].CandidateVer != 0) {
      TryToInstall InstallAction(Cache, &Fix, BrokenFix);
      InstallAction(Cache[Pkg].CandidateVerIter(Cache));
      InstallAction.doAutoInstall();
   } else
      return AllowFail;

   return true;
}


const char *BuildDepType(unsigned char const &Type)
{
   const char *fields[] = {
      "Build-Depends",
      "Build-Depends-Indep",
      "Build-Conflicts",
      "Build-Conflicts-Indep"
   };
   if (unlikely(Type >= sizeof(fields)/sizeof(fields[0])))
      return "";
   return fields[Type];
}


bool ParseFileDeb822(string File,
                     std::vector<pkgSrcRecords::Parser::BuildDepRec> &BuildDeps,
                     bool const &ArchOnly,
                     bool const &StripMultiArch)
{
   pkgTagSection Tags;
   pkgSrcRecords::Parser::BuildDepRec rec;
   const char *fields[] = {
      "Build-Depends",
      "Build-Depends-Indep",
      "Build-Conflicts",
      "Build-Conflicts-Indep"
   };

   BuildDeps.clear();

   // see if we can read the file
   _error->PushToStack();
   FileFd Fd(File, FileFd::ReadOnly);
   if (Fd.Failed()) {
      return false;
   }
   pkgTagFile Sources(&Fd);

   if (_error->PendingError() == true)
   {
      _error->RevertToStack();
      return false;
   }
   _error->MergeWithStack();
   
   // read step by step
   while (Sources.Step(Tags) == true)
   {
      for (unsigned int I = 0; I < 4; I++)
      {
         if (ArchOnly && (I == 1 || I == 3))
            continue;

         const char *Start, *Stop;
         if (Tags.Find(fields[I], Start, Stop) == false)
            continue;

         while (1)
         {
            Start = debListParser::ParseDepends(Start, Stop, 
               rec.Package,rec.Version,rec.Op,true,StripMultiArch,true);
	 
            if (Start == 0) 
               return _error->Error("Problem parsing dependency: %s", fields[I]);

            rec.Type = I;
            if (rec.Package != "")
            {
               BuildDeps.push_back(rec);
            }
	 
            if (Start == Stop) 
               break;
         }
      }
   }
   return true;
}


static bool DoBuildDep(CommandLine &CmdL)
{
   CacheFile Cache;

   _config->Set("APT::Install-Recommends", false);
   
   if (Cache.Open(true) == false)
      return false;

   if (CmdL.FileSize() == 0)
      return _error->Error(_("Must specify at least one source file to check builddeps for"));
   
   // Read the source list
   if (Cache.BuildSourceList() == false)
      return false;

   bool StripMultiArch;
   string hostArch = _config->Find("APT::Get::Host-Architecture");
   if (hostArch.empty() == false)
   {
      std::vector<std::string> archs = APT::Configuration::getArchitectures();
      if (std::find(archs.begin(), archs.end(), hostArch) == archs.end())
         return _error->Error(_("No architecture information available for %s. See apt.conf(5) APT::Architectures for setup"), hostArch.c_str());
      StripMultiArch = false;
   }
   else
      StripMultiArch = true;

   unsigned J = 0;
   for (const char **I = CmdL.FileList; *I != 0; I++, J++)
   {
      string Src = string(*I);

      // Process the build-dependencies
      vector<pkgSrcRecords::Parser::BuildDepRec> BuildDeps;

      // FIXME: Can't specify architecture to use for [wildcard] matching, so switch default arch temporary
      if (hostArch.empty() == false)
      {
         std::string nativeArch = _config->Find("APT::Architecture");
         _config->Set("APT::Architecture", hostArch);
         bool Success = ParseFileDeb822(*I, BuildDeps, _config->FindB("APT::Get::Arch-Only", false), StripMultiArch);
         _config->Set("APT::Architecture", nativeArch);
         if (Success == false)
            return _error->Error(_("Unable to get build-dependency information for %s"),Src.c_str());
      }
      else if (ParseFileDeb822(*I, BuildDeps, _config->FindB("APT::Get::Arch-Only", false), StripMultiArch) == false)
         return _error->Error(_("Unable to get build-dependency information for %s"),Src.c_str());

      // Also ensure that build-essential packages are present
      Configuration::Item const *Opts = _config->Tree("APT::Build-Essential");
      if (Opts) 
         Opts = Opts->Child;
      for (; Opts; Opts = Opts->Next)
      {
         if (Opts->Value.empty() == true)
            continue;

         pkgSrcRecords::Parser::BuildDepRec rec;
         rec.Package = Opts->Value;
         rec.Type = pkgSrcRecords::Parser::BuildDependIndep;
         rec.Op = 0;
         BuildDeps.push_back(rec);
      }
      
      if (BuildDeps.empty() == true)
      {
         ioprintf(c1out,_("%s has no build depends.\n"),Src.c_str());
         continue;
      }

      // Install the requested packages
      vector <pkgSrcRecords::Parser::BuildDepRec>::iterator D;
      pkgProblemResolver Fix(Cache);
      bool skipAlternatives = false; // skip remaining alternatives in an or group
      for (D = BuildDeps.begin(); D != BuildDeps.end(); ++D)
      {
         bool hasAlternatives = (((*D).Op & pkgCache::Dep::Or) == pkgCache::Dep::Or);

         if (skipAlternatives == true)
         {
            /*
             * if there are alternatives, we've already picked one, so skip
             * the rest
             *
             * TODO: this means that if there's a build-dep on A|B and B is
             * installed, we'll still try to install A; more importantly,
             * if A is currently broken, we cannot go back and try B. To fix 
             * this would require we do a Resolve cycle for each package we 
             * add to the install list. Ugh
             */
            if (!hasAlternatives)
               skipAlternatives = false; // end of or group
            continue;
         }

         if ((*D).Type == pkgSrcRecords::Parser::BuildConflict ||
             (*D).Type == pkgSrcRecords::Parser::BuildConflictIndep)
         {
            pkgCache::GrpIterator Grp = Cache->FindGrp((*D).Package);
            // Build-conflicts on unknown packages are silently ignored
            if (Grp.end() == true)
               continue;

            for (pkgCache::PkgIterator Pkg = Grp.PackageList(); Pkg.end() == false; Pkg = Grp.NextPkg(Pkg))
            {
               pkgCache::VerIterator IV = (*Cache)[Pkg].InstVerIter(*Cache);
               /*
                * Remove if we have an installed version that satisfies the
                * version criteria
                */
               if (IV.end() == false &&
                   Cache->VS().CheckDep(IV.VerStr(),(*D).Op,(*D).Version.c_str()) == true)
                  TryToInstallBuildDep(Pkg,Cache,Fix,true,false);
            }
         }
         else // BuildDep || BuildDepIndep
         {
            if (_config->FindB("Debug::BuildDeps",false) == true)
               cerr << "Looking for " << (*D).Package << "...\n";

            pkgCache::PkgIterator Pkg;

            // Cross-Building?
            if (StripMultiArch == false && D->Type != pkgSrcRecords::Parser::BuildDependIndep)
            {
               size_t const colon = D->Package.find(":");
               if (colon != string::npos)
               {
                  if (strcmp(D->Package.c_str() + colon, ":any") == 0 || strcmp(D->Package.c_str() + colon, ":native") == 0)
                     Pkg = Cache->FindPkg(D->Package.substr(0,colon));
                  else
                     Pkg = Cache->FindPkg(D->Package);
               }
               else
                  Pkg = Cache->FindPkg(D->Package, hostArch);

               // a bad version either is invalid or doesn't satify dependency
               #define BADVER(Ver) (Ver.end() == true || \
                     (D->Version.empty() == false && \
                     Cache->VS().CheckDep(Ver.VerStr(),D->Op,D->Version.c_str()) == false))

               APT::VersionList verlist;
               if (Pkg.end() == false)
               {
                  pkgCache::VerIterator Ver = (*Cache)[Pkg].InstVerIter(*Cache);
                  if (BADVER(Ver) == false)
                     verlist.insert(Ver);
                  Ver = (*Cache)[Pkg].CandidateVerIter(*Cache);
                  if (BADVER(Ver) == false)
                     verlist.insert(Ver);
               }
               if (verlist.empty() == true)
               {
                  pkgCache::PkgIterator BuildPkg = Cache->FindPkg(D->Package, "native");
                  if (BuildPkg.end() == false && Pkg != BuildPkg)
                  {
                     pkgCache::VerIterator Ver = (*Cache)[BuildPkg].InstVerIter(*Cache);
                     if (BADVER(Ver) == false)
                        verlist.insert(Ver);
                     Ver = (*Cache)[BuildPkg].CandidateVerIter(*Cache);
                     if (BADVER(Ver) == false)
                        verlist.insert(Ver);
                  }
               }
               #undef BADVER

               string forbidden;
               // We need to decide if host or build arch, so find a version we can look at
               APT::VersionList::const_iterator Ver = verlist.begin();
               for (; Ver != verlist.end(); ++Ver)
               {
                  forbidden.clear();
                  if (Ver->MultiArch == pkgCache::Version::None || Ver->MultiArch == pkgCache::Version::All)
                  {
                     if (colon == string::npos)
                        Pkg = Ver.ParentPkg().Group().FindPkg(hostArch);
                     else if (strcmp(D->Package.c_str() + colon, ":any") == 0)
                        forbidden = "Multi-Arch: none";
                     else if (strcmp(D->Package.c_str() + colon, ":native") == 0)
                        Pkg = Ver.ParentPkg().Group().FindPkg("native");
                  }
                  else if (Ver->MultiArch == pkgCache::Version::Same)
                  {
                     if (colon == string::npos)
                        Pkg = Ver.ParentPkg().Group().FindPkg(hostArch);
                     else if (strcmp(D->Package.c_str() + colon, ":any") == 0)
                        forbidden = "Multi-Arch: same";
                     else if (strcmp(D->Package.c_str() + colon, ":native") == 0)
                        Pkg = Ver.ParentPkg().Group().FindPkg("native");
                  }
                  else if ((Ver->MultiArch & pkgCache::Version::Foreign) == pkgCache::Version::Foreign)
                  {
                     if (colon == string::npos)
                        Pkg = Ver.ParentPkg().Group().FindPkg("native");
                     else if (strcmp(D->Package.c_str() + colon, ":any") == 0 ||
                              strcmp(D->Package.c_str() + colon, ":native") == 0)
                        forbidden = "Multi-Arch: foreign";
                  }
                  else if ((Ver->MultiArch & pkgCache::Version::Allowed) == pkgCache::Version::Allowed)
                  {
                     if (colon == string::npos)
                        Pkg = Ver.ParentPkg().Group().FindPkg(hostArch);
                     else if (strcmp(D->Package.c_str() + colon, ":any") == 0)
                     {
                        // prefer any installed over preferred non-installed architectures
                        pkgCache::GrpIterator Grp = Ver.ParentPkg().Group();
                        // we don't check for version here as we are better of with upgrading than remove and install
                        for (Pkg = Grp.PackageList(); Pkg.end() == false; Pkg = Grp.NextPkg(Pkg))
                           if (Pkg.CurrentVer().end() == false)
                              break;
                        if (Pkg.end() == true)
                           Pkg = Grp.FindPreferredPkg(true);
                     }
                     else if (strcmp(D->Package.c_str() + colon, ":native") == 0)
                        Pkg = Ver.ParentPkg().Group().FindPkg("native");
                  }

                  if (forbidden.empty() == false)
                  {
                     if (_config->FindB("Debug::BuildDeps",false) == true)
                        cerr << D->Package.substr(colon, string::npos) << " is not allowed from " << forbidden << " package " << (*D).Package << " (" << Ver.VerStr() << ")" << endl;
                     continue;
                  }

                  //we found a good version
                  break;
               }
               if (Ver == verlist.end())
               {
                  if (_config->FindB("Debug::BuildDeps",false) == true)
                     cerr << " No multiarch info as we have no satisfying installed nor candidate for " << D->Package << " on build or host arch" << endl;

                  if (forbidden.empty() == false)
                  {
                     if (hasAlternatives)
                        continue;
                     return _error->Error(_("%s dependency for %s can't be satisfied "
                                            "because %s is not allowed on '%s' packages"),
                                          BuildDepType(D->Type), Src.c_str(),
                                          D->Package.c_str(), forbidden.c_str());
                  }
               }
            }
            else
               Pkg = Cache->FindPkg(D->Package);

            if (Pkg.end() == true || (Pkg->VersionList == 0 && Pkg->ProvidesList == 0))
            {
               if (_config->FindB("Debug::BuildDeps",false) == true)
                  cerr << " (not found)" << (*D).Package << endl;

               if (hasAlternatives)
                  continue;

               return _error->Error(_("%s dependency for %s cannot be satisfied "
                                      "because the package %s cannot be found"),
                                    BuildDepType((*D).Type),Src.c_str(),
                                    (*D).Package.c_str());
            }

            pkgCache::VerIterator IV = (*Cache)[Pkg].InstVerIter(*Cache);
            if (IV.end() == false)
            {
               if (_config->FindB("Debug::BuildDeps",false) == true)
                  cerr << "  Is installed\n";

               if (D->Version.empty() == true ||
                   Cache->VS().CheckDep(IV.VerStr(),(*D).Op,(*D).Version.c_str()) == true)
               {
                  skipAlternatives = hasAlternatives;
                  continue;
               }

               if (_config->FindB("Debug::BuildDeps",false) == true)
                  cerr << "    ...but the installed version doesn't meet the version requirement\n";

               if (((*D).Op & pkgCache::Dep::LessEq) == pkgCache::Dep::LessEq)
                  return _error->Error(_("Failed to satisfy %s dependency for %s: Installed package %s is too new"),
                                       BuildDepType((*D).Type), Src.c_str(), Pkg.FullName(true).c_str());
            }

            // Only consider virtual packages if there is no versioned dependency
            if ((*D).Version.empty() == true)
            {
               /*
                * If this is a virtual package, we need to check the list of
                * packages that provide it and see if any of those are
                * installed
                */
               pkgCache::PrvIterator Prv = Pkg.ProvidesList();
               for (; Prv.end() != true; ++Prv)
               {
                  if (_config->FindB("Debug::BuildDeps",false) == true)
                     cerr << "  Checking provider " << Prv.OwnerPkg().FullName() << endl;

                  if ((*Cache)[Prv.OwnerPkg()].InstVerIter(*Cache).end() == false)
                     break;
               }

               if (Prv.end() == false)
               {
                  if (_config->FindB("Debug::BuildDeps",false) == true)
                     cerr << "  Is provided by installed package " << Prv.OwnerPkg().FullName() << endl;
                  skipAlternatives = hasAlternatives;
                  continue;
               }
            }
            else // versioned dependency
            {
               pkgCache::VerIterator CV = (*Cache)[Pkg].CandidateVerIter(*Cache);
               if (CV.end() == true ||
                  Cache->VS().CheckDep(CV.VerStr(),(*D).Op,(*D).Version.c_str()) == false)
               {
                  if (hasAlternatives)
                     continue;
                  else if (CV.end() == false)
                     return _error->Error(_("%s dependency for %s cannot be satisfied "
                                            "because candidate version of package %s "
                                            "can't satisfy version requirements"),
                                          BuildDepType(D->Type), Src.c_str(),
                                          D->Package.c_str());
                  else
                     return _error->Error(_("%s dependency for %s cannot be satisfied "
                                            "because package %s has no candidate version"),
                                          BuildDepType(D->Type), Src.c_str(),
                                          D->Package.c_str());
               }
            }

            if (TryToInstallBuildDep(Pkg,Cache,Fix,false,false,false) == true)
            {
               // We successfully installed something; skip remaining alternatives
               skipAlternatives = hasAlternatives;
               if (_config->FindB("APT::Get::Build-Dep-Automatic", false) == true)
                  Cache->MarkAuto(Pkg, true);
               continue;
            }
            else if (hasAlternatives)
            {
               if (_config->FindB("Debug::BuildDeps",false) == true)
                  cerr << "  Unsatisfiable, trying alternatives\n";
               continue;
            }
            else
            {
               return _error->Error(_("Failed to satisfy %s dependency for %s: %s"),
                                    BuildDepType((*D).Type),
                                    Src.c_str(),
                                    (*D).Package.c_str());
            }
         }
      }

      if (Fix.Resolve(true) == false)
         _error->Discard();
      
      // Now we check the state of the packages,
      if (Cache->BrokenCount() != 0)
      {
         ShowBroken(cerr, Cache, false);
         return _error->Error(_("Build-dependencies for %s could not be satisfied."),*I);
      }
   }

   for (unsigned J = 0; J < Cache->Head().PackageCount; J++)
   {
      pkgCache::PkgIterator I(Cache,Cache.List[J]);
      if (Cache[I].NewInstall() == true)
      {
         cout << I.FullName(true)
              << ":"
              << Cache[I].CandidateVerIter(Cache).Arch()
              << "="
              << string(Cache[I].CandVersion)
              << endl;
      }
   }

   return true;
}


static bool ShowHelp(CommandLine &)
{
   ioprintf(cerr,_("%s %s for %s compiled on %s %s\n"),PACKAGE,PACKAGE_VERSION,
                 COMMON_ARCH,__DATE__,__TIME__);
	    
   if (_config->FindB("version") == true)
   {
      cerr << _("Supported modules:") << endl;
      
      for (unsigned I = 0; I != pkgVersioningSystem::GlobalListLen; I++)
      {
         pkgVersioningSystem *VS = pkgVersioningSystem::GlobalList[I];
         if (_system != 0 && _system->VS == VS)
            cerr << '*';
         else
            cerr << ' ';
         cerr << "Ver: " << VS->Label << endl;
	 
         /* Print out all the packaging systems that will work with 
            this VS */
         for (unsigned J = 0; J != pkgSystem::GlobalListLen; J++)
         {
            pkgSystem *Sys = pkgSystem::GlobalList[J];
            if (_system == Sys)
               cerr << '*';
            else
               cerr << ' ';
            if (Sys->VS->TestCompatibility(*VS) == true)
               cerr << "Pkg:  " << Sys->Label << " (Priority " << Sys->Score(*_config) << ")" << endl;
         }
      }
      
      for (unsigned I = 0; I != pkgSourceList::Type::GlobalListLen; I++)
      {
         pkgSourceList::Type *Type = pkgSourceList::Type::GlobalList[I];
         cerr << " S.L: '" << Type->Name << "' " << Type->Label << endl;
      }

      for (unsigned I = 0; I != pkgIndexFile::Type::GlobalListLen; I++)
      {
         pkgIndexFile::Type *Type = pkgIndexFile::Type::GlobalList[I];
         cerr << " Idx: " << Type->Label << endl;
      }      
      
      return true;
   }

   cerr << _(
      "Usage: apt-resolve-dep [options] controlfile\n"
      "\n"
      "Resolve build dependencies from a control file (or .dsc file).\n"
      "\n"
      "Options:\n"
      "  -a                       host architecture\n"
      "  -P                       build profiles\n"
      "  -c, --config-file=VALUE  read this configuration file\n"
      "  -o, --option=VALUE       set an arbitrary configuration option, eg -o dir::cache=/tmp\n"
      "      --version            print version number\n"
      "  -h, --help               show this help\n"
      "\n"
      "See https://github.com/grosskur/apt-resolve-dep for updates.\n"
   );
   return true;
}


int main(int argc,const char *argv[])
{
   std::vector<CommandLine::Args> Args;
   Args.reserve(50);

   addArg('a', "host-architecture", "APT::Get::Host-Architecture", CommandLine::HasArg);
   addArg('P', "build-profiles", "APT::Build-Profiles", CommandLine::HasArg);
   addArg(0, "purge", "APT::Get::Purge", 0);
   addArg(0, "solver", "APT::Solver", CommandLine::HasArg);

   // options without a command
   addArg('h', "help", "help", 0);
   addArg(0, "version", "version", 0);
   // general options
   addArg('c', "config-file", 0, CommandLine::ConfigFile);
   addArg('o', "option", 0, CommandLine::ArbItem);
   addArg(0, NULL, NULL, 0);

   // Set up gettext support
   setlocale(LC_ALL,"");
   textdomain(PACKAGE);

   // Parse the command line and initialize the package library
   CommandLine CmdL(Args.data(),_config);
   if (pkgInitConfig(*_config) == false ||
       CmdL.Parse(argc,argv) == false ||
       pkgInitSystem(*_config,_system) == false)
   {
      if (_config->FindB("version") == true)
         ShowHelp(CmdL);
	 
      _error->DumpErrors();
      return 100;
   }
   _config->Set("quiet", 2);
   _config->Set("APT::Get::Simulate", true);
   _config->Set("Debug::NoLocking", true);

   // See if the help should be shown
   if (_config->FindB("help") == true ||
       _config->FindB("version") == true)
   {
      ShowHelp(CmdL);
      return 0;
   }

   if (CmdL.FileSize() == 0)
   {
      cerr << "error: no control files specific" << endl;
      return 2;
   }

   // see if we are in simulate mode
   //CheckSimulateMode(CmdL);

   // Init the signals
   InitSignals();

   // Setup the output streams
   InitOutput();

   DoBuildDep(CmdL);

   // Print any errors or warnings found during parsing
   bool const Errors = _error->PendingError();
   if (_config->FindI("quiet",0) > 0)
      _error->DumpErrors();
   else
      _error->DumpErrors(GlobalError::DEBUG);
   return Errors == true ? 100 : 0;
}
