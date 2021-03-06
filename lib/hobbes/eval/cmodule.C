
#include "hobbes/eval/cc.H"
#include "hobbes/eval/cmodule.H"
#include "hobbes/lang/module.H"
#include "hobbes/lang/type.H"
#include "hobbes/lang/typeinf.H"
#include "hobbes/lang/preds/class.H"
#include "hobbes/util/str.H"
#include "hobbes/util/array.H"

#include <stdexcept>
#include <dlfcn.h>
#include <glob.h>

namespace hobbes {

bool fileExists(const std::string& fname) {
  // not the most elegant, but it does the job
  FILE* f = fopen(fname.c_str(), "r");
  if (!f) return false;
  fclose(f);
  return true;
}

bool importObject(cc* e, const std::string& sopath) {
  if (!fileExists(sopath)) {
    return false;
  } else {
    void* h = dlopen(sopath.c_str(), RTLD_NOW);
    if (!h) { throw std::runtime_error(std::string("Failed to load .so file: ") + dlerror()); }

    typedef void (*InitF)(cc*);
    InitF initF = (InitF)dlsym(h, "initialize");

    if (!initF) { dlclose(h); throw std::runtime_error(std::string("Failed to load .so file init: ") + dlerror()); }

    initF((cc*)e);
    return true;
  }
}

bool importScript(cc* e, const std::string& fname) {
  if (!fileExists(fname)) {
    return false;
  } else {
    compile(e, ((cc*)e)->readModuleFile(fname));
    return true;
  }
}

typedef std::vector<std::string> ModulePaths;
static ModulePaths& modulePaths() {
  static thread_local ModulePaths mps;
  if (mps.size()==0) {
    mps.push_back(".");
  }
  return mps;
}
void pushModuleDir(const std::string& d) { modulePaths().push_back(d); }
void popModuleDir() { if (modulePaths().size() > 0) modulePaths().resize(modulePaths().size()-1); }

// import a "module" from a path spec (A.B.C => [.|$MODULEPATH]/A/B/C.*)
void import(cc* e, const std::string& mname) {
  for (size_t p = modulePaths().size(); p > 0; --p) {
    std::string path  = modulePaths()[p-1];
    std::string mpath = (path.empty() ? "." : path) + "/" + str::replace<char>(mname, ".", "/");

    if (importObject(e, mpath + ".so")) {
      return;
    } else {
      for (const auto& p : str::paths(mpath + ".*")) {
        if (importScript(e, p)) {
          return;
        }
      }
    }
  }
  throw std::runtime_error("No such module to load: " + mname);
}

// index type variables and sanity check names to ensure no duplicates
typedef std::map<std::string, int> NameIndexing;

NameIndexing nameIndexing(const str::seq& ns) {
  NameIndexing r;
  for (int i = 0; i < ns.size(); ++i) {
    if (r.find(ns[i]) != r.end()) {
      throw std::runtime_error("Duplicate name '" + ns[i] + "'");
    } else {
      r[ns[i]] = i;
    }
  }
  return r;
}

NameIndexing nameIndexing(const std::set<std::string>& ns) {
  return nameIndexing(str::seq(ns.begin(), ns.end()));
}

int nameIndex(const NameIndexing& ns, const std::string& vn) {
  NameIndexing::const_iterator ni = ns.find(vn);
  if (ni == ns.end()) {
    throw std::runtime_error("Undefined type name, '" + vn + "'");
  } else {
    return ni->second;
  }
}

std::vector<int> nameIndex(const NameIndexing& ns, const str::seq& vns) {
  std::vector<int> r;
  for (str::seq::const_iterator vn = vns.begin(); vn != vns.end(); ++vn) {
    r.push_back(nameIndex(ns, *vn));
  }
  return r;
}

MonoTypeSubst substitution(const NameIndexing& ns) {
  MonoTypeSubst s;
  for (NameIndexing::const_iterator ni = ns.begin(); ni != ns.end(); ++ni) {
    s[ni->first] = MonoTypePtr(TGen::make(ni->second));
  }
  return s;
}

MonoTypeSubst uvarSubstitution(const NameIndexing& ns) {
  MonoTypeSubst s;
  for (NameIndexing::const_iterator ni = ns.begin(); ni != ns.end(); ++ni) {
    s[ni->first] = freshTypeVar();
  }
  return s;
}

// convert functional dependencies into index form
void resolveNames(const NameIndexing& ns, const CFunDepDef& nfdep, FunDeps* out) {
  VarIDs lhs = nameIndex(ns, nfdep.first);
  for (str::seq::const_iterator vn = nfdep.second.begin(); vn != nfdep.second.end(); ++vn) {
    out->push_back(FunDep(lhs, nameIndex(ns, *vn)));
  }
}

FunDeps resolveNames(const NameIndexing& ns, const CFunDepDefs& nfdeps) {
  FunDeps r;
  for (CFunDepDefs::const_iterator fd = nfdeps.begin(); fd != nfdeps.end(); ++fd) {
    resolveNames(ns, *fd, &r);
  }
  return r;
}

// convert member definitions
TClass::Members resolveMembers(const MonoTypeSubst& s, const MVarTypeDefs& mvtds) {
  TClass::Members r;
  for (const auto& mvtd : mvtds) {
    if (r.find(mvtd->varName()) != r.end()) {
      throw annotated_error(*mvtd, "Duplicate class member name, '" + mvtd->varName() + "'");
    } else {
      r[mvtd->varName()] = requireMonotype(substitute(s, mvtd->varType()));
    }
  }
  return r;
}

// make a type class
void compile(cc* e, const ClassDef* cd) {
  try {
    NameIndexing    tns  = nameIndexing(cd->vars());
    MonoTypeSubst   s    = substitution(tns);
    Constraints     reqs = substitute(s, cd->constraints());
    FunDeps         fds  = resolveNames(tns, cd->fundeps());
    TClass::Members mems = resolveMembers(s, cd->members());

    e->typeEnv()->bind(cd->name(), UnqualifierPtr(new TClass(reqs, cd->name(), tns.size(), mems, mergeFundeps(inferFundeps(e->typeEnv(), reqs), fds), cd->la())));
  } catch (std::exception& ex) {
    throw annotated_error(*cd, ex.what());
  }
}

// compile class instance member definitions
MemberMapping compileMembers(MonoTypeUnifier* u, const TClassPtr& c, const MonoTypes& targs, cc* e, const MVarDefs& ds, bool asfn) {
  // Class X => Instance Y, X unify Y applied to class member types should yield instance member types
  MonoTypes cargs = freshTypeVars(c->typeVars()); 
  mgu(targs, cargs, u);

  // compile each member symbol binding
  MemberMapping ms;
  for (const auto& d : ds) {
    std::string n = d->varWithArgs()[0];

    if (ms.find(n) != ms.end()) {
      throw annotated_error(*d, "Duplicate instance member name, '" + n + "'");
    }

    // generate the expression for the member definition
    bool gendef = d->varWithArgs().size() > 1;
    ExprPtr mexp;
    if (gendef) {
      Fn::VarNames vns(d->varWithArgs().begin() + 1, d->varWithArgs().end());
      mexp = ExprPtr(new Fn(vns, d->varExpr(), d->la()));
    } else {
      mexp = d->varExpr();
    }

    // determine how to store this member depending on whether we're making an instance function,
    // or if we're generating a new function for a ground instance
    if (asfn) {
      ms[n] = mexp;
    } else {
      MonoTypePtr expectedMemberType = instantiate(u->substitute(cargs), c->memberType(n));

      if (!gendef) {
        ms[n] = e->normalize(assume(mexp, expectedMemberType, mexp->la()));
      } else {
        std::string fn = "." + n + freshName();
        e->define(fn, assume(mexp, expectedMemberType, mexp->la()));
        ms[n] = e->normalize(ExprPtr(new Var(fn, mexp->la())));
      }
    }
  }
  return ms;
}

// make a type class instance
void compile(cc* e, const InstanceDef* id) {
  try {
    UnqualifierPtr tyc = e->typeEnv()->lookupUnqualifier(id->className());
    TClassPtr      c   = std::dynamic_pointer_cast<TClass>(tyc);
    
    if (c.get() == 0) {
      throw std::runtime_error("Cannot define overload in '" + id->className() + "', class does not exist.");
    }

    MonoTypeUnifier u(e->typeEnv());

    NameIndexing  tns   = nameIndexing(tvarNames(id->args()));
    MonoTypes     targs = id->args();
    bool          asfn  = id->constraints().size() > 0 || tvarNames(targs).size() > 0;
    MemberMapping ms    = compileMembers(&u, c, targs, e, id->members(), asfn);

    // is this a ground instance or an instance function?
    if (!asfn) {
      Definitions ds;
      try {
        c->insert(e->typeEnv(), TCInstancePtr(new TCInstance(id->className(), targs, ms, id->la())), &ds);
        e->drainUnqualifyDefs(ds);
      } catch (...) {
        e->drainUnqualifyDefs(ds);
        throw;
      }
    } else {
      c->insert(TCInstanceFnPtr(new TCInstanceFn(id->className(), id->constraints(), targs, ms, id->la())));
    }
  } catch (std::exception& ex) {
    throw annotated_error(*id, ex.what());
  }
}

// compile import statements
void compile(cc* e, const MImport* mimp) {
  pushModuleDir(mimp->path());
  try {
    import(e, mimp->name());
    popModuleDir();
  } catch (std::exception&) {
    popModuleDir();
    throw;
  }
}

// compile type definitions
void compile(cc* e, const MTypeDef* mtd) {
  // TODO: evaluate these type definitions along with regular module compilation
  //       (currently these are only handled at parse-time)
  /*
  if (e->isTypeAliasName(mtd->name())) {
    throw std::runtime_error("The type '" + mtd->name() + "' is already defined, can't introduce new definition: " + show(mtd));
  } else {
    e->defineTypeAlias(mtd->name(), mtd->arguments(), mtd->type());
  }
  */
}

// compile regular variable definitions
void compile(cc* e, const MVarDef* mvd) {
  if (mvd->varWithArgs().size() == 1) {
    e->define(mvd->varWithArgs()[0], mvd->varExpr());
  } else {
    Fn::VarNames vns(mvd->varWithArgs().begin() + 1, mvd->varWithArgs().end());
    e->define(mvd->varWithArgs()[0], ExprPtr(new Fn(vns, mvd->varExpr(), mvd->la())));
  }
}

// compile forward-declarations
void compile(cc* e, const MVarTypeDef* vtd) {
  try {
    e->forwardDeclare(vtd->varName(), vtd->varType());
  } catch (std::exception& ex) {
    throw annotated_error(*vtd, ex.what());
  }
}

// for now, just treat each definition independently and stick it in the input environment
//   (this disallows things like mutual recursion)
void compile(cc* e, const ModulePtr& m) {
  for (ModuleDefs::const_iterator md = m->definitions().begin(); md != m->definitions().end(); ++md) {
    if (const MImport* imp = is<MImport>(*md)) {
      compile(e, imp);
    } else if (const ClassDef* cd = is<ClassDef>(*md)) {
      compile(e, cd);
    } else if (const InstanceDef* id = is<InstanceDef>(*md)) {
      compile(e, id);
    } else if (const MTypeDef* td = is<MTypeDef>(*md)) {
      compile(e, td);
    } else if (const MVarDef* vd = is<MVarDef>(*md)) {
      compile(e, vd);
    } else if (const MVarTypeDef* vtd = is<MVarTypeDef>(*md)) {
      compile(e, vtd);
    } else {
      throw std::runtime_error("Cannot compile module definition: " + show(*md));
    }
  }
}

}

