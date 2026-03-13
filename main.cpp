/*
 * EasyLang (EL1) Interpreter
 * A human-readable, English-like scripting language
 * Version: EL1
 * Fast, optimized, easy to use
 */

// Windows (MSVC / MinGW) needs _USE_MATH_DEFINES BEFORE <cmath>
// to expose M_PI, M_E, M_SQRT2, etc.
#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <variant>
#include <regex>
#include <chrono>
#include <algorithm>
#include <cmath>

// Fallback definitions for M_PI / M_E in case the compiler
// still doesn't provide them (some older MinGW / strict MSVC builds)
#ifndef M_PI
#define M_PI   3.14159265358979323846
#endif
#ifndef M_E
#define M_E    2.71828182845904523536
#endif
#ifndef M_SQRT2
#define M_SQRT2 1.41421356237309504880
#endif
#include <stdexcept>
#include <memory>
#include <optional>
#include <cassert>
#include <thread>
#include <ctime>
#include <cstring>
#include <sys/stat.h>

// ──────────────────────────────────────────────
//  Forward declarations
// ──────────────────────────────────────────────
struct Value;
struct Scope;
using ValueRef = std::shared_ptr<Value>;
using ScopeRef = std::shared_ptr<Scope>;

// ──────────────────────────────────────────────
//  Value type
// ──────────────────────────────────────────────
enum class VType { Nil, Bool, Number, String, Table, Function };

struct FunctionDef {
    std::vector<std::string> params;
    std::vector<std::string> body;
    ScopeRef closure;
};

struct Value {
    VType type = VType::Nil;
    bool   b   = false;
    double n   = 0.0;
    std::string s;
    std::vector<ValueRef> arr;                       // array part of table
    std::unordered_map<std::string,ValueRef> tbl;    // hash part of table
    std::shared_ptr<FunctionDef> fn;

    // factories
    static ValueRef nil()           { return std::make_shared<Value>(); }
    static ValueRef boolean(bool v) { auto x=std::make_shared<Value>(); x->type=VType::Bool;   x->b=v; return x; }
    static ValueRef number(double v){ auto x=std::make_shared<Value>(); x->type=VType::Number; x->n=v; return x; }
    static ValueRef string(const std::string& v){ auto x=std::make_shared<Value>(); x->type=VType::String; x->s=v; return x; }
    static ValueRef table()         { auto x=std::make_shared<Value>(); x->type=VType::Table; return x; }
    static ValueRef function(std::shared_ptr<FunctionDef> f){
        auto x=std::make_shared<Value>(); x->type=VType::Function; x->fn=f; return x;
    }

    bool truthy() const {
        switch(type){
            case VType::Nil:    return false;
            case VType::Bool:   return b;
            case VType::Number: return n != 0.0;
            case VType::String: return !s.empty();
            default:            return true;
        }
    }

    std::string repr() const {
        switch(type){
            case VType::Nil:    return "nil";
            case VType::Bool:   return b ? "true" : "false";
            case VType::Number: {
                if(n == std::floor(n) && std::abs(n)<1e15)
                    return std::to_string((long long)n);
                std::ostringstream ss; ss<<n; return ss.str();
            }
            case VType::String: return s;
            case VType::Table: {
                std::string out="[table: ";
                out+=std::to_string(arr.size())+" items, ";
                out+=std::to_string(tbl.size())+" keys]";
                return out;
            }
            case VType::Function: return "[function]";
        }
        return "nil";
    }

    bool operator==(const Value& o) const {
        if(type!=o.type) return false;
        switch(type){
            case VType::Nil:    return true;
            case VType::Bool:   return b==o.b;
            case VType::Number: return n==o.n;
            case VType::String: return s==o.s;
            default:            return this==&o;
        }
    }
};

// ──────────────────────────────────────────────
//  Scope / environment
// ──────────────────────────────────────────────
struct Scope {
    std::unordered_map<std::string,ValueRef> vars;
    ScopeRef parent;

    Scope(ScopeRef p=nullptr): parent(p){}

    ValueRef get(const std::string& name) const {
        auto it=vars.find(name);
        if(it!=vars.end()) return it->second;
        if(parent) return parent->get(name);
        return Value::nil();
    }
    void set(const std::string& name, ValueRef val){
        // walk up to find existing binding
        Scope* s=this;
        while(s){
            auto it=s->vars.find(name);
            if(it!=s->vars.end()){ it->second=val; return; }
            s=s->parent.get();
        }
        vars[name]=val;
    }
    void define(const std::string& name, ValueRef val){ vars[name]=val; }
};

// ──────────────────────────────────────────────
//  Helpers
// ──────────────────────────────────────────────
static std::string trim(const std::string& s){
    size_t a=s.find_first_not_of(" \t\r\n");
    if(a==std::string::npos) return "";
    size_t b=s.find_last_not_of(" \t\r\n");
    return s.substr(a,b-a+1);
}

static std::string toLower(std::string s){
    std::transform(s.begin(),s.end(),s.begin(),::tolower);
    return s;
}

static std::vector<std::string> splitWords(const std::string& s){
    std::vector<std::string> out;
    std::istringstream ss(s);
    std::string w;
    while(ss>>w) out.push_back(w);
    return out;
}

static bool isNumber(const std::string& s){
    if(s.empty()) return false;
    char* e;
    std::strtod(s.c_str(),&e);
    return *e=='\0';
}

// ──────────────────────────────────────────────
//  Tokeniser for expressions
// ──────────────────────────────────────────────
enum class TokKind { Num, Str, Ident, Plus, Minus, Star, Slash, Percent,
                     EqEq, NotEq, Lt, Le, Gt, Ge, And, Or, Not,
                     LParen, RParen, LBrack, RBrack, Dot, Comma, EOF_T };

struct Token { TokKind kind; std::string text; double num=0; };

class Lexer {
    std::string src; size_t pos=0;
public:
    Lexer(const std::string& s):src(s){}
    char peek(){ return pos<src.size()?src[pos]:'\0'; }
    char get() { return pos<src.size()?src[pos++]:'\0'; }

    Token next(){
        while(pos<src.size() && std::isspace((unsigned char)src[pos])) pos++;
        if(pos>=src.size()) return {TokKind::EOF_T,""};
        char c=peek();
        // string literal
        if(c=='"'||c=='\''){
            char q=get(); std::string s;
            while(pos<src.size()&&peek()!=q){
                char ch=get();
                if(ch=='\\'){
                    char e=get();
                    switch(e){ case 'n': s+='\n'; break; case 't': s+='\t'; break; default: s+=e; }
                } else s+=ch;
            }
            if(pos<src.size()) get();
            return {TokKind::Str,s};
        }
        // number
        if(std::isdigit((unsigned char)c)||( c=='-' && pos+1<src.size() && std::isdigit((unsigned char)src[pos+1]) )){
            std::string ns; if(c=='-'){ns+=get();} 
            while(pos<src.size()&&(std::isdigit((unsigned char)peek())||peek()=='.')){ns+=get();}
            double d=std::stod(ns);
            return {TokKind::Num,ns,d};
        }
        // ident / keyword
        if(std::isalpha((unsigned char)c)||c=='_'){
            std::string id;
            while(pos<src.size()&&(std::isalnum((unsigned char)peek())||peek()=='_')){id+=get();}
            std::string lo=toLower(id);
            if(lo=="and") return {TokKind::And,"and"};
            if(lo=="or")  return {TokKind::Or,"or"};
            if(lo=="not") return {TokKind::Not,"not"};
            return {TokKind::Ident,id};
        }
        get();
        switch(c){
            case '+': return {TokKind::Plus,"+"};
            case '-': return {TokKind::Minus,"-"};
            case '*': return {TokKind::Star,"*"};
            case '/': return {TokKind::Slash,"/"};
            case '%': return {TokKind::Percent,"%"};
            case '(': return {TokKind::LParen,"("};
            case ')': return {TokKind::RParen,")"};
            case '[': return {TokKind::LBrack,"["};
            case ']': return {TokKind::RBrack,"]"};
            case '.': return {TokKind::Dot,"."};
            case ',': return {TokKind::Comma,","};
            case '=':
                if(peek()=='='){ get(); return {TokKind::EqEq,"=="}; }
                return {TokKind::EqEq,"="};
            case '!':
                if(peek()=='='){ get(); return {TokKind::NotEq,"!="}; }
                return {TokKind::Not,"!"};
            case '<':
                if(peek()=='='){ get(); return {TokKind::Le,"<="}; }
                return {TokKind::Lt,"<"};
            case '>':
                if(peek()=='='){ get(); return {TokKind::Ge,">="}; }
                return {TokKind::Gt,">"};
            default:
                return {TokKind::EOF_T,""};
        }
    }

    std::vector<Token> tokenize(){
        std::vector<Token> toks;
        while(true){
            auto t=next();
            toks.push_back(t);
            if(t.kind==TokKind::EOF_T) break;
        }
        return toks;
    }
};

// ──────────────────────────────────────────────
//  Expression parser (Pratt)
// ──────────────────────────────────────────────
class ExprParser {
    std::vector<Token> toks;
    size_t pos=0;
    ScopeRef scope;
    std::unordered_map<std::string,ValueRef>& defines;

    Token peek(){ return pos<toks.size()?toks[pos]:Token{TokKind::EOF_T,""}; }
    Token consume(){ return pos<toks.size()?toks[pos++]:Token{TokKind::EOF_T,""}; }
    bool check(TokKind k){ return peek().kind==k; }
    bool match(TokKind k){ if(check(k)){consume();return true;}return false; }

public:
    ExprParser(const std::string& expr, ScopeRef s, std::unordered_map<std::string,ValueRef>& defs)
        : scope(s), defines(defs)
    {
        Lexer lex(expr);
        toks=lex.tokenize();
    }

    ValueRef parse(){ return parseOr(); }

    ValueRef parseOr(){
        auto left=parseAnd();
        while(peek().kind==TokKind::Or){
            consume();
            auto right=parseAnd();
            left=Value::boolean(left->truthy()||right->truthy());
        }
        return left;
    }

    ValueRef parseAnd(){
        auto left=parseNot();
        while(peek().kind==TokKind::And){
            consume();
            auto right=parseNot();
            left=Value::boolean(left->truthy()&&right->truthy());
        }
        return left;
    }

    ValueRef parseNot(){
        if(peek().kind==TokKind::Not){ consume(); return Value::boolean(!parseNot()->truthy()); }
        return parseCompare();
    }

    ValueRef parseCompare(){
        auto left=parseAdd();
        while(true){
            auto k=peek().kind;
            if(k==TokKind::EqEq){ consume(); auto r=parseAdd(); left=Value::boolean(*left==*r); }
            else if(k==TokKind::NotEq){ consume(); auto r=parseAdd(); left=Value::boolean(!(*left==*r)); }
            else if(k==TokKind::Lt){ consume(); auto r=parseAdd(); left=Value::boolean(left->n<r->n); }
            else if(k==TokKind::Le){ consume(); auto r=parseAdd(); left=Value::boolean(left->n<=r->n); }
            else if(k==TokKind::Gt){ consume(); auto r=parseAdd(); left=Value::boolean(left->n>r->n); }
            else if(k==TokKind::Ge){ consume(); auto r=parseAdd(); left=Value::boolean(left->n>=r->n); }
            else break;
        }
        return left;
    }

    ValueRef parseAdd(){
        auto left=parseMul();
        while(true){
            if(peek().kind==TokKind::Plus){
                consume(); auto r=parseMul();
                if(left->type==VType::String||r->type==VType::String)
                    left=Value::string(left->repr()+r->repr());
                else left=Value::number(left->n+r->n);
            } else if(peek().kind==TokKind::Minus){
                consume(); auto r=parseMul();
                left=Value::number(left->n-r->n);
            } else break;
        }
        return left;
    }

    ValueRef parseMul(){
        auto left=parseUnary();
        while(true){
            if(peek().kind==TokKind::Star){ consume(); auto r=parseUnary(); left=Value::number(left->n*r->n); }
            else if(peek().kind==TokKind::Slash){ consume(); auto r=parseUnary(); 
                if(r->n==0) throw std::runtime_error("division by zero");
                left=Value::number(left->n/r->n); }
            else if(peek().kind==TokKind::Percent){ consume(); auto r=parseUnary(); left=Value::number(std::fmod(left->n,r->n)); }
            else break;
        }
        return left;
    }

    ValueRef parseUnary(){
        if(peek().kind==TokKind::Minus){ consume(); return Value::number(-parsePrimary()->n); }
        return parsePostfix(parsePrimary());
    }

    ValueRef parsePostfix(ValueRef base){
        while(true){
            if(peek().kind==TokKind::Dot){
                consume();
                auto key=consume().text;
                if(base->type==VType::Table){
                    auto it=base->tbl.find(key);
                    if(it!=base->tbl.end()) base=it->second;
                    else base=Value::nil();
                } else base=Value::nil();
            } else if(peek().kind==TokKind::LBrack){
                consume();
                auto idx=parseOr();
                if(!match(TokKind::RBrack)) throw std::runtime_error("expected ]");
                if(base->type==VType::Table){
                    if(idx->type==VType::Number){
                        size_t i=(size_t)idx->n;
                        if(i<base->arr.size()) base=base->arr[i];
                        else base=Value::nil();
                    } else {
                        auto it=base->tbl.find(idx->s);
                        if(it!=base->tbl.end()) base=it->second;
                        else base=Value::nil();
                    }
                } else base=Value::nil();
            } else break;
        }
        return base;
    }

    ValueRef parsePrimary(){
        auto t=peek();
        if(t.kind==TokKind::Num){ consume(); return Value::number(t.num); }
        if(t.kind==TokKind::Str){ consume(); return Value::string(t.text); }
        if(t.kind==TokKind::LParen){
            consume(); auto v=parseOr();
            if(!match(TokKind::RParen)) throw std::runtime_error("expected )");
            return v;
        }
        if(t.kind==TokKind::Ident){
            consume();
            std::string lo=toLower(t.text);
            if(lo=="true")  return Value::boolean(true);
            if(lo=="false") return Value::boolean(false);
            if(lo=="nil"||lo=="nothing"||lo=="null") return Value::nil();
            // check defines
            auto dit=defines.find(lo);
            if(dit!=defines.end()) return dit->second;
            // scope lookup
            return scope->get(t.text);
        }
        return Value::nil();
    }
};

// ──────────────────────────────────────────────
//  Interpreter
// ──────────────────────────────────────────────
struct ReturnSignal { ValueRef val; };
struct BreakSignal  {};
struct ContinueSignal {};
struct ExitSignal   { int code; };

class Interpreter {
public:
    bool noConsole=false;
    bool optimize=false;
    bool faster=false;

    std::unordered_map<std::string,ValueRef> defines;
    std::unordered_map<std::string,FunctionDef> globalFunctions;
    ScopeRef globalScope;

    Interpreter(){
        globalScope=std::make_shared<Scope>();
        setupBuiltins();
    }

    void setupBuiltins(){
        // math constants
        defines["pi"]=Value::number(M_PI);
        defines["e"] =Value::number(M_E);
        defines["tau"]=Value::number(2*M_PI);
        defines["infinity"]=Value::number(std::numeric_limits<double>::infinity());
        defines["true_val"]=Value::boolean(true);
        defines["false_val"]=Value::boolean(false);
    }

    // ── Run a file (lines already loaded) ──
    void runLines(const std::vector<std::string>& lines, ScopeRef scope, size_t start=0, size_t end=0){
        if(end==0) end=lines.size();
        size_t i=start;
        while(i<end){
            try {
                i=executeLine(lines, i, end, scope);
            } catch(ReturnSignal&){ throw; }
            catch(BreakSignal&){ throw; }
            catch(ContinueSignal&){ throw; }
            catch(ExitSignal&){ throw; }
            catch(std::exception& ex){
                std::cerr<<"[EL1 Error] Line "<<(i+1)<<": "<<ex.what()<<"\n";
                i++;
            }
        }
    }

    // Returns next line index to execute
    size_t executeLine(const std::vector<std::string>& lines, size_t i, size_t end, ScopeRef scope){
        std::string raw=lines[i];
        std::string line=trim(raw);

        // blank / comment
        if(line.empty()||line[0]=='#'||line.substr(0,2)=="//"){
            return i+1;
        }

        // get indent level of NEXT block (for block detection)
        std::string lo=toLower(line);
        auto words=splitWords(lo);
        if(words.empty()) return i+1;

        // ── define ──────────────────────────────
        // define PI as 3.14159
        if(words[0]=="define"&&words.size()>=4&&words[2]=="as"){
            std::string name=toLower(words[1]);
            std::string valStr=line.substr(line.find(" as ")+4);
            defines[name]=evalExpr(trim(valStr),scope);
            return i+1;
        }

        // ── let ──────────────────────────────────
        // let x be 10
        // let x be "hello"
        if(words[0]=="let"&&words.size()>=4&&words[2]=="be"){
            // Skip special "let X be math/length/type of" forms (handled below)
            bool isSpecial=(words.size()>=5 && (words[3]=="math"||words[3]=="length"||words[3]=="type"));
            if(!isSpecial){
                std::string varName=words[1];
                std::string valPart=line.substr(line.find(" be ")+4);
                scope->define(varName, evalExpr(trim(valPart), scope));
                return i+1;
            }
        }

        // ── set (reassign) ───────────────────────
        // set x to 20
        if(words[0]=="set"&&words.size()>=4&&words[2]=="to"){
            std::string varName=words[1];
            std::string valPart=line.substr(line.find(" to ")+4);
            scope->set(varName, evalExpr(trim(valPart), scope));
            return i+1;
        }

        // ── change x by +5 / -5 ─────────────────
        if(words[0]=="change"&&words.size()>=4&&words[2]=="by"){
            std::string varName=words[1];
            std::string delta=words[3];
            auto cur=scope->get(varName);
            auto dv=evalExpr(delta,scope);
            scope->set(varName, Value::number(cur->n + dv->n));
            return i+1;
        }

        // ── increase / decrease ─────────────────
        if((words[0]=="increase"||words[0]=="decrease")&&words.size()>=4&&words[2]=="by"){
            std::string varName=words[1];
            auto cur=scope->get(varName);
            auto dv=evalExpr(words[3],scope);
            double result = words[0]=="increase" ? cur->n+dv->n : cur->n-dv->n;
            scope->set(varName, Value::number(result));
            return i+1;
        }

        // ── say ──────────────────────────────────
        // say "Hello World"
        if(words[0]=="say"&&!noConsole){
            // find "say " in original line (case-insensitive match already done above)
            size_t sayPos=lo.find("say ");
            std::string rest=(sayPos!=std::string::npos)?line.substr(sayPos+4):line.substr(4);
            auto v=evalExpr(trim(rest),scope);
            std::cout<<v->repr()<<"\n";
            return i+1;
        }
        if(words[0]=="say"&&noConsole) return i+1;

        // ── ask ──────────────────────────────────
        // ask "What is your name?" into name
        if(words[0]=="ask"&&!noConsole){
            size_t intoPos=lo.find(" into ");
            std::string prompt=line.substr(3,intoPos-3+3);
            if(intoPos!=std::string::npos){
                std::string varName=trim(line.substr(intoPos+6));
                std::cout<<evalExpr(trim(prompt),scope)->repr();
                std::string inp; std::getline(std::cin,inp);
                // try number
                if(isNumber(inp)) scope->set(varName,Value::number(std::stod(inp)));
                else scope->set(varName,Value::string(inp));
            }
            return i+1;
        }

        // ── create table ─────────────────────────
        // create table scores
        if(words[0]=="create"&&words[1]=="table"&&words.size()>=3){
            scope->define(words[2], Value::table());
            return i+1;
        }

        // ── add to table ─────────────────────────
        // add 42 to scores
        // add "key" -> "val" to scores
        if(words[0]=="add"&&words.size()>=4){
            size_t toIdx=std::find(words.begin(),words.end(),"to")-words.begin();
            if(toIdx<words.size()){
                std::string tblName=words[toIdx+1];
                auto tbl=scope->get(tblName);
                if(tbl->type==VType::Table){
                    std::string rawLine=trim(line.substr(4));
                    size_t arrowPos=rawLine.find("->");
                    if(arrowPos!=std::string::npos){
                        std::string kstr=trim(rawLine.substr(0,arrowPos));
                        size_t t2=rawLine.find(" to ");
                        std::string vstr=trim(rawLine.substr(arrowPos+2,t2-(arrowPos+2)));
                        auto k=evalExpr(kstr,scope);
                        auto v=evalExpr(vstr,scope);
                        tbl->tbl[k->repr()]=v;
                    } else {
                        size_t t2=rawLine.find(" to ");
                        std::string vstr=trim(rawLine.substr(0,t2));
                        tbl->arr.push_back(evalExpr(vstr,scope));
                    }
                }
            }
            return i+1;
        }

        // ── remove from table ────────────────────
        // remove 0 from scores
        if(words[0]=="remove"&&words.size()>=4&&words[2]=="from"){
            auto tbl=scope->get(words[3]);
            if(tbl->type==VType::Table){
                auto idx=evalExpr(words[1],scope);
                if(idx->type==VType::Number){
                    size_t ii=(size_t)idx->n;
                    if(ii<tbl->arr.size()) tbl->arr.erase(tbl->arr.begin()+ii);
                } else {
                    tbl->tbl.erase(idx->s);
                }
            }
            return i+1;
        }

        // ── function definition ──────────────────
        // make function greet with name do
        //   say "Hello " + name
        // done
        if((words[0]=="make"&&words[1]=="function")||(words[0]=="function")){
            size_t nameIdx = words[0]=="make" ? 2 : 1;
            std::string fnName=words[nameIdx];
            std::vector<std::string> params;
            // collect params after "with"
            size_t withIdx=std::find(words.begin(),words.end(),"with")-words.begin();
            size_t doIdx  =std::find(words.begin(),words.end(),"do")  -words.begin();
            if(withIdx<doIdx){
                for(size_t p=withIdx+1;p<doIdx;p++){
                    std::string pw=words[p];
                    if(pw!="and"&&pw!=","&&!pw.empty()) params.push_back(pw);
                }
            }
            // collect body until matching "done"
            int depth=1; size_t bi=i+1;
            std::vector<std::string> body;
            while(bi<end&&depth>0){
                std::string blo=toLower(trim(lines[bi]));
                auto bw=splitWords(blo);
                if(!bw.empty()){
                    if(isBlockStart(bw)) depth++;
                    if(bw[0]=="done"||bw[0]=="end") depth--;
                }
                if(depth>0) body.push_back(lines[bi]);
                bi++;
            }
            FunctionDef fd; fd.params=params; fd.body=body; fd.closure=scope;
            globalFunctions[fnName]=fd;
            scope->define(fnName,Value::function(std::make_shared<FunctionDef>(fd)));
            return bi;
        }

        // ── return ───────────────────────────────
        if(words[0]=="return"||words[0]=="give back"){
            std::string rest=line.substr(words[0]=="return"?6:9);
            throw ReturnSignal{evalExpr(trim(rest),scope)};
        }

        // ── call function / run ───────────────────
        // run greet with "Bob"
        // call greet with name and "!"
        if(words[0]=="run"||words[0]=="call"){
            std::string fnName=words[1];
            std::vector<ValueRef> args;
            size_t withP=lo.find(" with ");
            if(withP!=std::string::npos){
                std::string argStr=trim(line.substr(withP+6));
                args=parseArgList(argStr,scope);
            }
            callFunction(fnName,args,scope);
            return i+1;
        }

        // ── repeat N times ──────────────────────
        // repeat 5 times do
        //   say "hello"
        // done
        if(words[0]=="repeat"){
            auto countVal=evalExpr(words[1],scope);
            int count=(int)countVal->n;
            size_t bi=i+1;
            int depth=1;
            std::vector<std::string> body;
            while(bi<end&&depth>0){
                std::string blo=toLower(trim(lines[bi]));
                auto bw=splitWords(blo);
                if(!bw.empty()){
                    if(isBlockStart(bw)) depth++;
                    if(bw[0]=="done"||bw[0]=="end") depth--;
                }
                if(depth>0) body.push_back(lines[bi]);
                bi++;
            }
            for(int c=0;c<count;c++){
                auto loopScope=std::make_shared<Scope>(scope);
                loopScope->define("iteration",Value::number(c+1));
                try { runLines(body,loopScope); }
                catch(BreakSignal&){ break; }
                catch(ContinueSignal&){ continue; }
            }
            return bi;
        }

        // ── loop while ───────────────────────────
        // loop while x is less than 10 do
        // loop while x < 10 do
        if(words[0]=="loop"&&words[1]=="while"){
            // condition: everything between "while" and "do"
            size_t dop=lo.rfind(" do");
            std::string condStr=trim(line.substr(lo.find("while")+5, dop-(lo.find("while")+5)));
            condStr=normalizeCondition(condStr);
            size_t bi=i+1; int depth=1;
            std::vector<std::string> body;
            while(bi<end&&depth>0){
                std::string blo=toLower(trim(lines[bi]));
                auto bw=splitWords(blo);
                if(!bw.empty()){
                    if(isBlockStart(bw)) depth++;
                    if(bw[0]=="done"||bw[0]=="end") depth--;
                }
                if(depth>0) body.push_back(lines[bi]);
                bi++;
            }
            int maxIter=10000000;
            while(evalExpr(condStr,scope)->truthy()&&maxIter-->0){
                auto ls=std::make_shared<Scope>(scope);
                try { runLines(body,ls); }
                catch(BreakSignal&){ break; }
                catch(ContinueSignal&){ continue; }
            }
            return bi;
        }

        // ── for each ─────────────────────────────
        // for each item in scores do
        if(words[0]=="for"&&words[1]=="each"){
            std::string itemVar=words[2];
            std::string tblName=words[4]; // in <tblName>
            auto tbl=scope->get(tblName);
            size_t bi=i+1; int depth=1;
            std::vector<std::string> body;
            while(bi<end&&depth>0){
                std::string blo=toLower(trim(lines[bi]));
                auto bw=splitWords(blo);
                if(!bw.empty()){
                    if(isBlockStart(bw)) depth++;
                    if(bw[0]=="done"||bw[0]=="end") depth--;
                }
                if(depth>0) body.push_back(lines[bi]);
                bi++;
            }
            if(tbl->type==VType::Table){
                for(auto& v:tbl->arr){
                    auto ls=std::make_shared<Scope>(scope);
                    ls->define(itemVar,v);
                    try { runLines(body,ls); }
                    catch(BreakSignal&){ break; }
                    catch(ContinueSignal&){ continue; }
                }
                for(auto& [k,v]:tbl->tbl){
                    auto ls=std::make_shared<Scope>(scope);
                    ls->define(itemVar,v);
                    ls->define("key",Value::string(k));
                    try { runLines(body,ls); }
                    catch(BreakSignal&){ break; }
                    catch(ContinueSignal&){ continue; }
                }
            }
            return bi;
        }

        // ── however if (conditional) ─────────────
        // however if number is 10 then do
        // however if number is not 10 then do
        // however if number is either 10 or 5 then do
        if(words[0]=="however"&&words[1]=="if"){
            return handleHoweverIf(lines,i,end,scope,lo,line);
        }

        // ── also if (else if) ────────────────────
        // also if x is 5 then do
        if(words[0]=="also"&&words[1]=="if"){
            // skip — only reached if previous "however if" was true, skip block
            return skipBlock(lines,i+1,end);
        }

        // ── otherwise (else) ─────────────────────
        if(words[0]=="otherwise"){
            return skipBlock(lines,i+1,end);
        }

        // ── stop / break / skip ──────────────────
        if(words[0]=="stop") throw BreakSignal{};
        if(words[0]=="skip") throw ContinueSignal{};

        // ── exit ─────────────────────────────────
        if(words[0]=="exit"){
            int code=0;
            if(words.size()>=3&&words[1]=="with") code=(int)evalExpr(words[2],scope)->n;
            throw ExitSignal{code};
        }

        // ── sleep ─────────────────────────────────
        // sleep 2 seconds
        if(words[0]=="sleep"){
            double ms=evalExpr(words[1],scope)->n;
            if(words.size()>=3&&(words[2]=="seconds"||words[2]=="second")) ms*=1000;
            std::this_thread::sleep_for(std::chrono::milliseconds((int)ms));
            return i+1;
        }

        // ── math shortcuts ────────────────────────
        // let result be math sqrt of 25
        if(words[0]=="let"&&words.size()>=5&&words[2]=="be"&&words[3]=="math"){
            std::string varName=words[1];
            std::string fn=words[4];
            // use original-case words for the argument
            auto origW=splitWords(line);
            std::string arg=origW.size()>=7?origW[6]:origW.size()>=6?origW[5]:"";
            auto v=evalExpr(arg,scope);
            double res=0;
            if(fn=="sqrt")  res=std::sqrt(v->n);
            else if(fn=="abs")   res=std::abs(v->n);
            else if(fn=="floor") res=std::floor(v->n);
            else if(fn=="ceil")  res=std::ceil(v->n);
            else if(fn=="round") res=std::round(v->n);
            else if(fn=="sin")   res=std::sin(v->n);
            else if(fn=="cos")   res=std::cos(v->n);
            else if(fn=="tan")   res=std::tan(v->n);
            else if(fn=="log")   res=std::log(v->n);
            else if(fn=="log2")  res=std::log2(v->n);
            else if(fn=="log10") res=std::log10(v->n);
            scope->define(varName,Value::number(res));
            return i+1;
        }

        // ── let X be length of Y ──────────────────
        if(words[0]=="let"&&words.size()>=6&&words[2]=="be"&&words[3]=="length"&&words[4]=="of"){
            auto v=scope->get(words[5]);
            double len=0;
            if(v->type==VType::String) len=(double)v->s.size();
            else if(v->type==VType::Table) len=(double)(v->arr.size()+v->tbl.size());
            scope->define(words[1],Value::number(len));
            return i+1;
        }

        // ── let X be type of Y ───────────────────
        if(words[0]=="let"&&words.size()>=6&&words[2]=="be"&&words[3]=="type"&&words[4]=="of"){
            auto v=scope->get(words[5]);
            std::string t;
            switch(v->type){
                case VType::Nil: t="nil"; break;
                case VType::Bool: t="bool"; break;
                case VType::Number: t="number"; break;
                case VType::String: t="string"; break;
                case VType::Table: t="table"; break;
                case VType::Function: t="function"; break;
            }
            scope->define(words[1],Value::string(t));
            return i+1;
        }

        // ── done / end (block closer) ─────────────
        if(words[0]=="done"||words[0]=="end") return i+1;

        // ── free-standing expression / function call
        // Might be: greet "Bob"   OR   scores[0]
        // Try to evaluate as expression
        try {
            evalExpr(line, scope);
        } catch(...) {}
        return i+1;
    }

    // ── however if handler ──────────────────────
    size_t handleHoweverIf(
        const std::vector<std::string>& lines,
        size_t i, size_t end,
        ScopeRef scope,
        const std::string& lo,
        const std::string& line)
    {
        // Parse the condition from: however if <cond> then do|exit
        // Determine action: "then do", "then exit", "then do not"
        bool exitOnTrue=false, exitOnFalse=false, doNot=false;

        std::string rest; // everything after "however if "
        rest=line.substr(line.find("if ")+3);

        // detect action
        if(lo.find(" then exit")!=std::string::npos){
            // "however if X is 10 then exit"
            exitOnTrue=true;
            rest=rest.substr(0,rest.rfind(" then exit"));
        } else if(lo.find(" then do not")!=std::string::npos){
            doNot=true;
            rest=rest.substr(0,rest.rfind(" then do not"));
        } else if(lo.find(" then do")!=std::string::npos){
            rest=rest.substr(0,rest.rfind(" then do"));
        }

        // Evaluate English-style condition
        bool cond=evalEnglishCondition(trim(rest),scope);
        if(doNot) cond=!cond;

        if(exitOnTrue&&cond) throw ExitSignal{0};

        // Collect then-block
        size_t bi=i+1; int depth=1;
        std::vector<std::string> thenBody;
        while(bi<end&&depth>0){
            std::string blo=toLower(trim(lines[bi]));
            auto bw=splitWords(blo);
            bool isElseIf=(!bw.empty()&&bw[0]=="also"&&bw.size()>1&&bw[1]=="if");
            bool isElse  =(!bw.empty()&&bw[0]=="otherwise");
            if(depth==1&&(isElseIf||isElse)) break;
            if(!bw.empty()){
                if(isBlockStart(bw)) depth++;
                if(bw[0]=="done"||bw[0]=="end") depth--;
            }
            if(depth>0) thenBody.push_back(lines[bi]);
            bi++;
        }

        // Try to collect else-if / else chains
        std::vector<std::pair<std::string,std::vector<std::string>>> elseIfChain;
        std::vector<std::string> elseBody;

        while(bi<end){
            std::string blo=toLower(trim(lines[bi]));
            auto bw=splitWords(blo);
            if(bw.empty()){ bi++; continue; }
            if(bw[0]=="also"&&bw.size()>1&&bw[1]=="if"){
                // also if ... then do
                std::string eiLine=lines[bi];
                std::string eiRest=eiLine.substr(eiLine.find("if ")+3);
                size_t td=blo.rfind(" then do");
                if(td!=std::string::npos) eiRest=eiRest.substr(0,eiRest.rfind(" then do"));
                int d2=1; bi++;
                std::vector<std::string> eiBody;
                while(bi<end&&d2>0){
                    std::string blo2=toLower(trim(lines[bi]));
                    auto bw2=splitWords(blo2);
                    bool isEI2=(!bw2.empty()&&bw2[0]=="also"&&bw2.size()>1&&bw2[1]=="if");
                    bool isE2 =(!bw2.empty()&&bw2[0]=="otherwise");
                    if(d2==1&&(isEI2||isE2)) break;
                    if(!bw2.empty()){
                        if(isBlockStart(bw2)) d2++;
                        if(bw2[0]=="done"||bw2[0]=="end") d2--;
                    }
                    if(d2>0) eiBody.push_back(lines[bi]);
                    bi++;
                }
                elseIfChain.push_back({trim(eiRest), eiBody});
            } else if(bw[0]=="otherwise"){
                int d2=1; bi++;
                while(bi<end&&d2>0){
                    std::string blo2=toLower(trim(lines[bi]));
                    auto bw2=splitWords(blo2);
                    if(!bw2.empty()){
                        if(isBlockStart(bw2)) d2++;
                        if(bw2[0]=="done"||bw2[0]=="end") d2--;
                    }
                    if(d2>0) elseBody.push_back(lines[bi]);
                    bi++;
                }
                break;
            } else break;
        }

        if(cond){
            runLines(thenBody,std::make_shared<Scope>(scope));
        } else {
            bool ran=false;
            for(auto& [eiCond,eiBody]:elseIfChain){
                if(evalEnglishCondition(eiCond,scope)){
                    runLines(eiBody,std::make_shared<Scope>(scope));
                    ran=true; break;
                }
            }
            if(!ran&&!elseBody.empty()){
                runLines(elseBody,std::make_shared<Scope>(scope));
            }
        }
        return bi;
    }

    // ── English condition parser ─────────────────
    // "number is 10"
    // "number is not 10"
    // "number is either 10 or 5"
    // "number is not 10 or 5 or 6"
    // "number is greater than 5"
    // "number is less than or equal to 5"
    // "x is true"  "name is empty"
    bool evalEnglishCondition(const std::string& raw, ScopeRef scope){
        std::string lo=toLower(trim(raw));
        auto words=splitWords(lo);
        if(words.empty()) return false;

        // find "is" keyword
        size_t isPos=std::find(words.begin(),words.end(),"is")-words.begin();
        if(isPos>=words.size()){
            // fallback: treat as expression
            return evalExpr(normalizeCondition(raw),scope)->truthy();
        }

        // LHS: join words before "is"
        std::string lhsStr;
        for(size_t x=0;x<isPos;x++) lhsStr+=words[x]+(x+1<isPos?" ":"");
        // get actual varname from original (preserve case)
        auto origWords=splitWords(raw);
        std::string varName=origWords[0]; // first word of original
        if(isPos>1){
            varName="";
            for(size_t x=0;x<isPos;x++) varName+=origWords[x]+(x+1<isPos?" ":"");
        }
        ValueRef lhs=evalExpr(varName,scope);

        // RHS words
        std::vector<std::string> rhs(words.begin()+isPos+1,words.end());
        std::vector<std::string> rhsOrig(origWords.begin()+isPos+1,origWords.end());

        if(rhs.empty()) return lhs->truthy();

        bool negated=false;
        size_t ri=0;
        if(ri<rhs.size()&&rhs[ri]=="not"){ negated=true; ri++; }

        // "is empty"
        if(ri<rhs.size()&&rhs[ri]=="empty"){
            bool empty=(lhs->type==VType::Nil)||
                       (lhs->type==VType::String&&lhs->s.empty())||
                       (lhs->type==VType::Table&&lhs->arr.empty()&&lhs->tbl.empty());
            return negated?!empty:empty;
        }

        // "is true" / "is false"
        if(ri<rhs.size()&&rhs[ri]=="true")  return negated?!lhs->truthy():lhs->truthy();
        if(ri<rhs.size()&&rhs[ri]=="false") return negated? lhs->truthy():!lhs->truthy();
        if(ri<rhs.size()&&rhs[ri]=="nil")   return negated?(lhs->type!=VType::Nil):(lhs->type==VType::Nil);

        // "is greater than" / "is less than" / "is greater than or equal to"
        if(ri<rhs.size()&&(rhs[ri]=="greater"||rhs[ri]=="less"||rhs[ri]=="more")){
            bool greater=(rhs[ri]=="greater"||rhs[ri]=="more");
            ri++;
            if(ri<rhs.size()&&rhs[ri]=="than") ri++;
            bool orEq=false;
            if(ri<rhs.size()&&rhs[ri]=="or"){ ri++;
                if(ri<rhs.size()&&rhs[ri]=="equal"){ ri++;
                    if(ri<rhs.size()&&rhs[ri]=="to") ri++;
                    orEq=true;
                }
            }
            std::string valStr;
            for(size_t x=ri;x<rhsOrig.size();x++) valStr+=rhsOrig[x]+(x+1<rhsOrig.size()?" ":"");
            auto rval=evalExpr(trim(valStr),scope);
            bool result=greater ? (orEq?lhs->n>=rval->n:lhs->n>rval->n)
                                : (orEq?lhs->n<=rval->n:lhs->n<rval->n);
            return negated?!result:result;
        }

        // "is either X or Y"
        if(ri<rhs.size()&&rhs[ri]=="either"){ ri++; }

        // "is X or Y or Z" or "is not X or Y"
        // collect values joined by "or"
        std::vector<std::string> candidates;
        std::string cur;
        for(size_t x=ri;x<rhs.size();x++){
            if(rhs[x]=="or"||rhs[x]=="and"){
                if(!cur.empty()) candidates.push_back(trim(cur));
                cur="";
            } else {
                cur+=rhsOrig[x]+" ";
            }
        }
        if(!cur.empty()) candidates.push_back(trim(cur));

        bool matched=false;
        for(auto& c:candidates){
            auto rv=evalExpr(c,scope);
            if(*lhs==*rv){ matched=true; break; }
        }
        return negated?!matched:matched;
    }

    // ── skip a block (used by else-if/else skip) ─
    size_t skipBlock(const std::vector<std::string>& lines, size_t i, size_t end){
        int depth=1;
        while(i<end&&depth>0){
            std::string blo=toLower(trim(lines[i]));
            auto bw=splitWords(blo);
            if(!bw.empty()){
                bool isEI=(!bw.empty()&&bw[0]=="also"&&bw.size()>1&&bw[1]=="if");
                bool isE =(!bw.empty()&&bw[0]=="otherwise");
                if(depth==1&&(isEI||isE)) return i;
                if(isBlockStart(bw)) depth++;
                if(bw[0]=="done"||bw[0]=="end") depth--;
            }
            i++;
        }
        return i;
    }

    bool isBlockStart(const std::vector<std::string>& w){
        if(w.empty()) return false;
        if(w[0]=="however"&&w.size()>1&&w[1]=="if") return true;
        if(w[0]=="also"&&w.size()>1&&w[1]=="if") return true;
        if(w[0]=="otherwise") return true;
        if(w[0]=="repeat") return true;
        if(w[0]=="loop") return true;
        if(w[0]=="for"&&w.size()>1&&w[1]=="each") return true;
        if(w[0]=="make"&&w.size()>1&&w[1]=="function") return true;
        if(w[0]=="function") return true;
        return false;
    }

    // ── normalise English condition to expr ──────
    std::string normalizeCondition(const std::string& s){
        std::string r=s;
        // Replace English operators
        auto rpl=[&](const std::string& from,const std::string& to){
            size_t p=0;
            while((p=r.find(from,p))!=std::string::npos){ r.replace(p,from.size(),to); p+=to.size(); }
        };
        rpl(" is equal to "," == ");
        rpl(" is not equal to "," != ");
        rpl(" is greater than or equal to "," >= ");
        rpl(" is less than or equal to "," <= ");
        rpl(" is greater than "," > ");
        rpl(" is less than "," < ");
        rpl(" is not "," != ");
        rpl(" is "," == ");
        return r;
    }

    // ── call a function ─────────────────────────
    ValueRef callFunction(const std::string& name, const std::vector<ValueRef>& args, ScopeRef scope){
        // built-ins
        if(name=="print"||name=="say"){
            for(auto& a:args) { if(!noConsole) std::cout<<a->repr(); }
            if(!noConsole) std::cout<<"\n";
            return Value::nil();
        }
        if(name=="input"||name=="ask"){
            if(!noConsole){
                if(!args.empty()) std::cout<<args[0]->repr();
                std::string inp; std::getline(std::cin,inp);
                if(isNumber(inp)) return Value::number(std::stod(inp));
                return Value::string(inp);
            }
            return Value::nil();
        }
        if(name=="tostring"){ return args.empty()?Value::string(""):Value::string(args[0]->repr()); }
        if(name=="tonumber"){ 
            if(args.empty()) return Value::number(0);
            if(isNumber(args[0]->repr())) return Value::number(std::stod(args[0]->repr()));
            return Value::nil();
        }
        if(name=="typeof"){
            if(args.empty()) return Value::string("nil");
            switch(args[0]->type){
                case VType::Nil: return Value::string("nil");
                case VType::Bool: return Value::string("bool");
                case VType::Number: return Value::string("number");
                case VType::String: return Value::string("string");
                case VType::Table: return Value::string("table");
                case VType::Function: return Value::string("function");
            }
        }
        if(name=="sqrt") return args.empty()?Value::number(0):Value::number(std::sqrt(args[0]->n));
        if(name=="abs")  return args.empty()?Value::number(0):Value::number(std::abs(args[0]->n));
        if(name=="floor")return args.empty()?Value::number(0):Value::number(std::floor(args[0]->n));
        if(name=="ceil") return args.empty()?Value::number(0):Value::number(std::ceil(args[0]->n));
        if(name=="round")return args.empty()?Value::number(0):Value::number(std::round(args[0]->n));
        if(name=="len"||name=="length"){
            if(args.empty()) return Value::number(0);
            if(args[0]->type==VType::String) return Value::number(args[0]->s.size());
            if(args[0]->type==VType::Table)  return Value::number(args[0]->arr.size()+args[0]->tbl.size());
            return Value::number(0);
        }
        if(name=="upper"){ return args.empty()?Value::nil():Value::string([]( std::string s){ std::transform(s.begin(),s.end(),s.begin(),::toupper);return s; }(args[0]->s)); }
        if(name=="lower"){ return args.empty()?Value::nil():Value::string([]( std::string s){ std::transform(s.begin(),s.end(),s.begin(),::tolower);return s; }(args[0]->s)); }
        if(name=="random"){
            double lo2=0,hi2=1;
            if(args.size()>=2){lo2=args[0]->n;hi2=args[1]->n;}
            else if(args.size()==1){hi2=args[0]->n;}
            return Value::number(lo2+(hi2-lo2)*((double)rand()/(double)RAND_MAX));
        }
        if(name=="time"||name=="now"){
            auto t=std::chrono::system_clock::now().time_since_epoch();
            return Value::number((double)std::chrono::duration_cast<std::chrono::milliseconds>(t).count());
        }

        // user-defined
        auto fv=scope->get(name);
        if(fv->type==VType::Function&&fv->fn){
            auto fnScope=std::make_shared<Scope>(fv->fn->closure);
            for(size_t p=0;p<fv->fn->params.size();p++){
                fnScope->define(fv->fn->params[p], p<args.size()?args[p]:Value::nil());
            }
            try {
                runLines(fv->fn->body,fnScope);
            } catch(ReturnSignal& r){ return r.val; }
            return Value::nil();
        }
        // global functions
        auto git=globalFunctions.find(name);
        if(git!=globalFunctions.end()){
            auto& fd=git->second;
            auto fnScope=std::make_shared<Scope>(fd.closure?fd.closure:globalScope);
            for(size_t p=0;p<fd.params.size();p++){
                fnScope->define(fd.params[p], p<args.size()?args[p]:Value::nil());
            }
            try {
                runLines(fd.body,fnScope);
            } catch(ReturnSignal& r){ return r.val; }
            return Value::nil();
        }
        return Value::nil();
    }

    // ── evaluate expression ──────────────────────
    ValueRef evalExpr(const std::string& expr, ScopeRef scope){
        std::string e=trim(expr);
        if(e.empty()) return Value::nil();

        // quick literals
        if(e=="nil"||e=="nothing"||e=="null") return Value::nil();
        if(e=="true") return Value::boolean(true);
        if(e=="false") return Value::boolean(false);
        if(isNumber(e)) return Value::number(std::stod(e));

        // check defines
        std::string lo=toLower(e);
        auto dit=defines.find(lo);
        if(dit!=defines.end()) return dit->second;

        // string literal — only match a true single-token string
        if(e.size()>=2 && ((e.front()=='"' && e.back()=='"') || (e.front()=='\'' && e.back()=='\''))) {
            bool singleToken=true;
            char q=e.front();
            for(size_t qi=1;qi<e.size()-1;qi++){
                if(e[qi]=='\\'){ qi++; continue; }
                if(e[qi]==q){ singleToken=false; break; }
            }
            if(singleToken){
                std::string out;
                for(size_t qi=1;qi<e.size()-1;qi++){
                    if(e[qi]=='\\'){ qi++;
                        if(qi<e.size()-1){
                            switch(e[qi]){ case 'n': out+='\n'; break; case 't': out+='\t'; break; default: out+=e[qi]; }
                        }
                    } else { out+=e[qi]; }
                }
                return Value::string(out);
            }
        }

        // function call with parens: name(arg1, arg2)
        {
            size_t lp=e.find('(');
            if(lp!=std::string::npos&&e.back()==')'){
                std::string fnName=trim(e.substr(0,lp));
                std::string argStr=e.substr(lp+1,e.size()-lp-2);
                auto args=parseArgList(argStr,scope);
                // check it's a valid identifier
                bool validId=!fnName.empty();
                for(char c:fnName) if(!std::isalnum((unsigned char)c)&&c!='_') validId=false;
                if(validId) return callFunction(fnName,args,scope);
            }
        }

        // full expression parser
        ExprParser ep(e, scope, defines);
        return ep.parse();
    }

    std::vector<ValueRef> parseArgList(const std::string& s, ScopeRef scope){
        std::vector<ValueRef> result;
        // split on commas and "and" outside quotes/parens
        int depth=0; bool inStr=false; char sq=0;
        std::string cur;
        for(size_t i=0;i<s.size();i++){
            char c=s[i];
            if(inStr){ cur+=c; if(c==sq) inStr=false; continue; }
            if(c=='"'||c=='\''){ inStr=true; sq=c; cur+=c; continue; }
            if(c=='('||c=='[') depth++;
            if(c==')'||c==']') depth--;
            if(depth==0&&c==','){
                std::string t=trim(cur);
                if(!t.empty()) result.push_back(evalExpr(t,scope));
                cur="";
            } else {
                cur+=c;
            }
        }
        std::string t=trim(cur);
        if(!t.empty()) result.push_back(evalExpr(t,scope));
        return result;
    }

    void runFile(const std::string& filename){
        std::ifstream f(filename);
        if(!f){ std::cerr<<"[EL1] Cannot open file: "<<filename<<"\n"; return; }
        std::vector<std::string> lines;
        std::string line;
        while(std::getline(f,line)) lines.push_back(line);
        try {
            runLines(lines,globalScope);
        }
        catch(ExitSignal& e){ std::exit(e.code); }
    }
};

// ──────────────────────────────────────────────
//  Main entry point
// ──────────────────────────────────────────────
int main(int argc, char* argv[]){
    if(argc<2){
        std::cout<<"EasyLang (EL1) Interpreter\n";
        std::cout<<"Usage: el <script.el> [options]\n";
        std::cout<<"Options:\n";
        std::cout<<"  -noconsole   Suppress all output\n";
        std::cout<<"  -faster      Skip startup checks\n";
        std::cout<<"  -optimize    Enable optimizer hints\n";
        std::cout<<"  -version     Print version info\n";
        return 0;
    }

    std::string scriptFile;
    bool noConsole=false,faster=false,optimize=false;

    for(int a=1;a<argc;a++){
        std::string arg=argv[a];
        if(arg=="-noconsole") noConsole=true;
        else if(arg=="-faster") faster=true;
        else if(arg=="-optimize") optimize=true;
        else if(arg=="-version"){ std::cout<<"EasyLang EL1 v1.0.0\n"; return 0; }
        else if(arg[0]!='-') scriptFile=arg;
    }

    if(scriptFile.empty()){ std::cerr<<"[EL1] No script file provided.\n"; return 1; }

    // check file exists and has .el extension
    struct stat st;
    if(stat(scriptFile.c_str(),&st)!=0){ std::cerr<<"[EL1] File not found: "<<scriptFile<<"\n"; return 1; }

    Interpreter interp;
    interp.noConsole=noConsole;
    interp.faster=faster;
    interp.optimize=optimize;

    if(!faster){
        // optional: print header
    }

    srand((unsigned)std::time(nullptr));
    interp.runFile(scriptFile);
    return 0;
}



