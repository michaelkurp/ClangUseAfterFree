/* Invocation of dread powers vvvvvvvv */
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"

#include "llvm/Support/CommandLine.h"

#include <iostream>
#include <map>
#include <set>
#include <string>
/* Invocation of the dread powers ^^^^^^^^^^*/

using namespace clang;
using namespace clang::tooling;
using namespace std;
using namespace llvm;

static llvm::cl::OptionCategory MyToolCategory("my-tool options");
static cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);
static cl::extrahelp MoreHelp("\nMore help text...\n");

class LoopsVisitor : public RecursiveASTVisitor<LoopsVisitor> {
  ASTContext *_context;
  bool inFunc = false;
  bool inCall = false;
  bool inFree = false;
  bool inDelete = false;
  bool ignoreIf = false;
  bool inIf = false;
  bool terminateBin = false;
  map<string, bool> nullPointers;
  set<std::string> Pointers;
public:

  explicit LoopsVisitor(ASTContext *context) : _context(context) {}

  ~LoopsVisitor(){}

  bool TraverseCallExpr(CallExpr* expr)
  {
    if(!ignoreIf){
      clang::LangOptions LangOpts;
      clang::PrintingPolicy Policy(LangOpts);
      string name = expr->getDirectCallee()->getNameInfo().getName().getAsString();
      if(name == "free")
      {
        inFree = true;
        string TypeS;
        llvm::raw_string_ostream s(TypeS);
        expr->getArg(0)->printPretty(s,0,Policy);
        std::string arg = s.str();
        if(Pointers.find(arg) != Pointers.end())
        {
          Pointers.erase(arg);
        }
      }
  }
    RecursiveASTVisitor<LoopsVisitor>::TraverseCallExpr(expr);
    inFree = false;
    return true;
  }

  bool TraverseCXXDeleteExpr(CXXDeleteExpr* expr)
  {
    if(!ignoreIf){
      expr->dump();
      clang::LangOptions LangOpts;
      clang::PrintingPolicy Policy(LangOpts);
      inDelete = true;
      string TypeS;
      llvm::raw_string_ostream s(TypeS);
      expr->getArgument()->printPretty(s,0,Policy);
      std::string arg = s.str();
      if(Pointers.find(arg) != Pointers.end())
      {
        Pointers.erase(arg);
      }
    }
    RecursiveASTVisitor<LoopsVisitor>::TraverseCXXDeleteExpr(expr);
    inDelete = false;
    return true;
  }
  bool VisitDeclRefExpr(DeclRefExpr* expr)
  {
    if(ignoreIf)
      return true;
    if(!inFree && !inDelete){
      if(VarDecl *VD = dyn_cast<VarDecl>(expr->getDecl())){
        if(VD->getType()->isPointerType() || VD->getType()->isArrayType()) {
          string name = VD->getNameAsString();
          if(Pointers.find(name) == Pointers.end())
          {
            std::cerr << "Possible Use-After-Free ";
            FullSourceLoc FullLocation = _context->getFullLoc(expr->getBeginLoc());
            int lineNumber = FullLocation.getSpellingLineNumber();
            cout << "at line: " << lineNumber << endl;
          }
        }
      }
  }
  return true;
  }
  //Add Branch Prediction Heuristics Here
  bool TraverseIfStmt(IfStmt *stmt){
    inIf = true;
    Expr *expr = stmt->getCond();
    //expr->dump();
    if(IntegerLiteral* lit = dyn_cast<IntegerLiteral>(expr))
    {
      if(*(lit->getValue().getRawData()) == 0)
      {
        ignoreIf = true;
      }
    }
    RecursiveASTVisitor<LoopsVisitor>::TraverseIfStmt(stmt);
    ignoreIf = false;
    inIf = false;
    return true;
  }
  bool VisitVarDecl(VarDecl *decl)
  {
    if(ignoreIf)
      return true;
    if(inFunc){
    string name = decl->getNameAsString();
    if(decl->getType()->isPointerType())
    {
      Pointers.insert(name);
    }
  }
    return true;
  }
  bool VisitBinaryOperator(BinaryOperator* op)
  {
    //More branch prediction
    if(inIf){
      auto s = op->getOpcodeStr();
      string opName = s.str();
      if(opName != "==" && opName != "!=")
        if(opName != ">" && opName != "<")
          if(opName != ">=" && opName != "<=")
            return true;

      Expr* lhs = op->getLHS();
      Expr* rhs = op->getRHS();
      if(IntegerLiteral* lit1 = dyn_cast<IntegerLiteral>(lhs))
      {
        if(IntegerLiteral* lit2 = dyn_cast<IntegerLiteral>(rhs))
        {
          int litInt1 = *(lit1->getValue().getRawData());
          int litInt2 = *(lit2->getValue().getRawData());
          std::cerr << litInt1 << opName << litInt2 << endl;
          if(opName == "==")
          {
            terminateBin = (litInt1 == litInt2);
          }
          else if(opName == "!=")
          {
            terminateBin = (litInt1 != litInt2);
          }
          else if(opName == ">")
          {
            terminateBin = (litInt1 > litInt2);
          }
          else if(opName == "<")
          {
            terminateBin = (litInt1 < litInt2);
          }
          else if(opName == ">=")
          {
            terminateBin = (litInt1 >= litInt2);
          }
          else if(opName != "<=")
          {
            terminateBin = (litInt1 <= litInt2);
          }
          ignoreIf = !terminateBin;
        }
      }
    }
//Skeleton code I was working on to keep a running list of NULL pointers to help with branch prediction
    /*
    else{
      Expr* lhs = op->getLHS();
      Expr* rhs = op->getRHS();

      clang::LangOptions LangOpts;
      clang::PrintingPolicy Policy(LangOpts);
      string TypeS;
      llvm::raw_string_ostream s(TypeS);
      rhs->getArgument()->printPretty(s,0,Policy);
      std::string arg = s.str();
      if(arg == "NULL")
      {
        if(DeclRefExpr* d = dyn_cast<DeclRefExpr>(lhs))
        {
          if(VarDecl* v = dyn_cast<VarDecl>(d))
          {
          }
        }
      }

    }
    */
    return true;
  }
  //
  bool TraverseFunctionDecl(FunctionDecl* decl)
  {
    inFunc = true;
    RecursiveASTVisitor<LoopsVisitor>::TraverseFunctionDecl(decl);
    inFunc = false;
    return true;
  }
};

class LoopsConsumer : public clang::ASTConsumer{
  LoopsVisitor _visitor;

public:
  explicit LoopsConsumer(ASTContext *context) : _visitor(context) {}

  virtual void HandleTranslationUnit(clang::ASTContext& Context){
    _visitor.TraverseDecl(Context.getTranslationUnitDecl());
  }
};

class Loops : public clang::ASTFrontendAction{
public:
  virtual std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance& Compiler,
								llvm::StringRef InFile) {
    return std::unique_ptr<clang::ASTConsumer>(new LoopsConsumer(&Compiler.getASTContext()));
  }
};

int main(int argc, const char **argv) {
  CommonOptionsParser OptionsParser(argc, argv, MyToolCategory);
  ClangTool Tool(OptionsParser.getCompilations(),
                 OptionsParser.getSourcePathList());
  return Tool.run(newFrontendActionFactory<Loops>().get());
}
