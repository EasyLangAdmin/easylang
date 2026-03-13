/*
 * EasyLang (EL1) Interpreter
 * A human-readable, English-like scripting language
 * Version: EL1  v2.0  — Full Application Edition
 *
 * NEW in v2.0:
 *   - File I/O  (read file / write file / append to file / delete file)
 *   - String ops (split / join / contains / replace / trim / starts with / ends with)
 *   - JSON-like serialisation (encode table / decode json)
 *   - HTTP client  (fetch url / post to url)
 *   - Error handling  (try do / catch error / throw error)
 *   - Events / callbacks  (on event / trigger event)
 *   - Timers  (after N seconds do / every N seconds do)
 *   - Environment  (get env / set env / args table)
 *   - Process  (run command / get output of command)
 *   - Modules  (import "file.el" / export name)
 *   - Advanced table ops (sort table / reverse table / find in table / count in table)
 *   - Math extras (power / clamp / lerp / map range / random int)
 *   - String formatting (format "{}" with x)
 *   - Multi-line strings  (using backtick blocks)
 *   - Switch/match  (match x with ...)
 *   - Assert  (assert x is 5 or fail "msg")
 *   - Logging  (log "msg" / log warn "msg" / log error "msg")
 *   - GUI window (open window / add button / add label / add input / show window)
 *     — cross-platform via system dialog fallbacks when no GUI available
 */

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
#include <algorithm>
#include <cmath>

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
#include <cstdlib>
#include <cctype>
#include <climits>
#include <sys/stat.h>
#include <chrono>
#include <regex>
#include <variant>
#include <functional>
#include <map>
#include <set>
#include <queue>
#include <numeric>

#ifdef _WIN32
  #include <windows.h>
  #include <direct.h>
  #define popen  _popen
  #define pclose _pclose
  #define getcwd _getcwd
#else
  #include <unistd.h>
  #include <dirent.h>
#endif

// ─────────────────────────────────────────────────────────────
//  Forward declarations
// ─────────────────────────────────────────────────────────────
struct Value;
struct Scope;
using ValueRef = std::shared_ptr<Value>;
using ScopeRef = std::shared_ptr<Scope>;

// ─────────────────────────────────────────────────────────────
//  Value
// ─────────────────────────────────────────────────────────────
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
    std::vector<ValueRef> arr;
    std::unordered_map<std::string,ValueRef> tbl;
    std::shared_ptr<FunctionDef> fn;

    static ValueRef nil()            { return std::make_shared<Value>(); }
    static ValueRef boolean(bool v)  { auto x=std::make_shared<Value>(); x->type=VType::Bool;   x->b=v; return x; }
    static ValueRef number(double v) { auto x=std::make_shared<Value>(); x->type=VType::Number; x->n=v; return x; }
    static ValueRef string(const std::string& v){ auto x=std::make_shared<Value>(); x->type=VType::String; x->s=v; return x; }
    static ValueRef table()          { auto x=std::make_shared<Value>(); x->type=VType::Table;  return x; }
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

    std::string repr(bool quoted=false) const {
        switch(type){
            case VType::Nil:    return "nil";
            case VType::Bool:   return b ? "true" : "false";
            case VType::Number: {
                if(n == std::floor(n) && std::abs(n)<1e15)
                    return std::to_string((long long)n);
                std::ostringstream ss; ss<<n; return ss.str();
            }
            case VType::String: return quoted ? "\""+s+"\"" : s;
            case VType::Table: {
                // pretty print as JSON-like
                if(!arr.empty()){
                    std::string out="[";
                    for(size_t i=0;i<arr.size();i++){
                        if(i) out+=", ";
                        out+=arr[i]->repr(true);
                    }
                    return out+"]";
                }
                if(!tbl.empty()){
                    std::string out="{";
                    bool first=true;
                    for(auto& [k,v]:tbl){
                        if(!first) out+=", "; first=false;
                        out+="\""+k+"\": "+v->repr(true);
                    }
                    return out+"}";
                }
                return "[]";
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

// ─────────────────────────────────────────────────────────────
//  Scope
// ─────────────────────────────────────────────────────────────
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

// ─────────────────────────────────────────────────────────────
//  Helpers
// ─────────────────────────────────────────────────────────────
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
static std::string toUpper(std::string s){
    std::transform(s.begin(),s.end(),s.begin(),::toupper);
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
static std::string strReplace(std::string src, const std::string& from, const std::string& to){
    size_t p=0;
    while((p=src.find(from,p))!=std::string::npos){ src.replace(p,from.size(),to); p+=to.size(); }
    return src;
}
static std::vector<std::string> strSplit(const std::string& s, const std::string& delim){
    std::vector<std::string> out;
    size_t pos=0,found;
    while((found=s.find(delim,pos))!=std::string::npos){
        out.push_back(s.substr(pos,found-pos));
        pos=found+delim.size();
    }
    out.push_back(s.substr(pos));
    return out;
}

// ─────────────────────────────────────────────────────────────
//  JSON Encoder / Decoder (lightweight, no deps)
// ─────────────────────────────────────────────────────────────
static std::string encodeJson(ValueRef v){
    switch(v->type){
        case VType::Nil:    return "null";
        case VType::Bool:   return v->b?"true":"false";
        case VType::Number: {
            if(v->n==std::floor(v->n)&&std::abs(v->n)<1e15)
                return std::to_string((long long)v->n);
            std::ostringstream ss; ss<<v->n; return ss.str();
        }
        case VType::String: {
            std::string out="\"";
            for(char c:v->s){
                if(c=='"') out+="\\\"";
                else if(c=='\\') out+="\\\\";
                else if(c=='\n') out+="\\n";
                else if(c=='\t') out+="\\t";
                else out+=c;
            }
            return out+"\"";
        }
        case VType::Table: {
            if(!v->arr.empty()){
                std::string out="[";
                for(size_t i=0;i<v->arr.size();i++){
                    if(i) out+=",";
                    out+=encodeJson(v->arr[i]);
                }
                return out+"]";
            }
            std::string out="{";
            bool first=true;
            for(auto& [k,val]:v->tbl){
                if(!first) out+=","; first=false;
                out+="\""+k+"\":"+encodeJson(val);
            }
            return out+"}";
        }
        default: return "null";
    }
}

// Forward-declare decoder
ValueRef decodeJson(const std::string& src, size_t& pos);

static void skipWs(const std::string& s, size_t& p){
    while(p<s.size()&&std::isspace((unsigned char)s[p])) p++;
}

ValueRef decodeJson(const std::string& src, size_t& pos){
    skipWs(src,pos);
    if(pos>=src.size()) return Value::nil();
    char c=src[pos];
    if(c=='"'){
        pos++;
        std::string out;
        while(pos<src.size()&&src[pos]!='"'){
            if(src[pos]=='\\'){pos++;
                switch(src[pos]){
                    case 'n': out+='\n'; break; case 't': out+='\t'; break;
                    case '"': out+='"'; break;  case '\\': out+='\\'; break;
                    default: out+=src[pos];
                }
            } else out+=src[pos];
            pos++;
        }
        pos++; // closing "
        return Value::string(out);
    }
    if(c=='['){
        pos++; auto arr=Value::table();
        skipWs(src,pos);
        if(pos<src.size()&&src[pos]==']'){pos++;return arr;}
        while(pos<src.size()){
            arr->arr.push_back(decodeJson(src,pos));
            skipWs(src,pos);
            if(pos<src.size()&&src[pos]==',') pos++;
            else break;
        }
        if(pos<src.size()&&src[pos]==']') pos++;
        return arr;
    }
    if(c=='{'){
        pos++; auto obj=Value::table();
        skipWs(src,pos);
        if(pos<src.size()&&src[pos]=='}'){pos++;return obj;}
        while(pos<src.size()){
            skipWs(src,pos);
            auto key=decodeJson(src,pos);
            skipWs(src,pos);
            if(pos<src.size()&&src[pos]==':') pos++;
            auto val=decodeJson(src,pos);
            obj->tbl[key->s]=val;
            skipWs(src,pos);
            if(pos<src.size()&&src[pos]==',') pos++;
            else break;
        }
        if(pos<src.size()&&src[pos]=='}') pos++;
        return obj;
    }
    if(c=='t'&&src.substr(pos,4)=="true"){  pos+=4; return Value::boolean(true); }
    if(c=='f'&&src.substr(pos,5)=="false"){ pos+=5; return Value::boolean(false); }
    if(c=='n'&&src.substr(pos,4)=="null"){  pos+=4; return Value::nil(); }
    // number
    std::string ns;
    if(c=='-') ns+=src[pos++];
    while(pos<src.size()&&(std::isdigit((unsigned char)src[pos])||src[pos]=='.'||src[pos]=='e'||src[pos]=='E'||src[pos]=='+'||src[pos]=='-'))
        ns+=src[pos++];
    if(!ns.empty()&&isNumber(ns)) return Value::number(std::stod(ns));
    return Value::nil();
}

// ─────────────────────────────────────────────────────────────
//  Tokeniser
// ─────────────────────────────────────────────────────────────
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
        while(pos<src.size()&&std::isspace((unsigned char)src[pos])) pos++;
        if(pos>=src.size()) return {TokKind::EOF_T,""};
        char c=peek();
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
        if(std::isdigit((unsigned char)c)||( c=='-' && pos+1<src.size() && std::isdigit((unsigned char)src[pos+1]) )){
            std::string ns; if(c=='-'){ns+=get();}
            while(pos<src.size()&&(std::isdigit((unsigned char)peek())||peek()=='.')){ns+=get();}
            double d=std::stod(ns);
            return {TokKind::Num,ns,d};
        }
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
        while(true){ auto t=next(); toks.push_back(t); if(t.kind==TokKind::EOF_T) break; }
        return toks;
    }
};

// ─────────────────────────────────────────────────────────────
//  Signals
// ─────────────────────────────────────────────────────────────
struct ReturnSignal  { ValueRef val; };
struct BreakSignal   {};
struct ContinueSignal{};
struct ExitSignal    { int code; };
struct ThrowSignal   { std::string msg; ValueRef val; };

// ─────────────────────────────────────────────────────────────
//  Expression Parser (Pratt)
// ─────────────────────────────────────────────────────────────
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
        : scope(s), defines(defs){ Lexer lex(expr); toks=lex.tokenize(); }

    ValueRef parse(){ return parseOr(); }

    ValueRef parseOr(){
        auto left=parseAnd();
        while(peek().kind==TokKind::Or){ consume(); auto r=parseAnd(); left=Value::boolean(left->truthy()||r->truthy()); }
        return left;
    }
    ValueRef parseAnd(){
        auto left=parseNot();
        while(peek().kind==TokKind::And){ consume(); auto r=parseNot(); left=Value::boolean(left->truthy()&&r->truthy()); }
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
                consume(); auto r=parseMul(); left=Value::number(left->n-r->n);
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
                consume(); auto key=consume().text;
                if(base->type==VType::Table){
                    auto it=base->tbl.find(key);
                    base=it!=base->tbl.end()?it->second:Value::nil();
                } else base=Value::nil();
            } else if(peek().kind==TokKind::LBrack){
                consume(); auto idx=parseOr();
                if(!match(TokKind::RBrack)) throw std::runtime_error("expected ]");
                if(base->type==VType::Table){
                    if(idx->type==VType::Number){
                        size_t i2=(size_t)idx->n;
                        base=i2<base->arr.size()?base->arr[i2]:Value::nil();
                    } else {
                        auto it=base->tbl.find(idx->s);
                        base=it!=base->tbl.end()?it->second:Value::nil();
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
            std::string lo2=toLower(t.text);
            if(lo2=="true")  return Value::boolean(true);
            if(lo2=="false") return Value::boolean(false);
            if(lo2=="nil"||lo2=="nothing"||lo2=="null") return Value::nil();
            // scope variables take priority over defines
            auto v1=scope->get(t.text);
            if(v1->type!=VType::Nil) return v1;
            auto v2=scope->get(lo2);
            if(v2->type!=VType::Nil) return v2;
            // fall back to defines (constants like pi, e, tau)
            auto dit=defines.find(lo2);
            if(dit!=defines.end()) return dit->second;
            return Value::nil();
        }
        return Value::nil();
    }
};

// ─────────────────────────────────────────────────────────────
//  Interpreter
// ─────────────────────────────────────────────────────────────
class Interpreter {
public:
    bool noConsole=false;
    bool optimize=false;
    bool faster=false;

    std::unordered_map<std::string,ValueRef> defines;
    std::unordered_map<std::string,FunctionDef> globalFunctions;
    std::unordered_map<std::string,std::vector<std::vector<std::string>>> eventHandlers;
    std::string scriptDir;
    std::set<std::string> importedFiles;
    ScopeRef globalScope;
    // GUI state (text-mode fallback)
    struct GuiWidget {
        std::string type; // button, label, input, title
        std::string id;
        std::string text;
        std::string callbackFn;
    };
    struct GuiWindow {
        std::string title;
        std::vector<GuiWidget> widgets;
        bool shown=false;
    };
    std::unordered_map<std::string,GuiWindow> windows;

    Interpreter(){ globalScope=std::make_shared<Scope>(); setupBuiltins(); }

    void setupBuiltins(){
        defines["pi"]       =Value::number(M_PI);
        defines["e"]        =Value::number(M_E);
        defines["tau"]      =Value::number(2*M_PI);
        defines["sqrt2"]    =Value::number(M_SQRT2);
        defines["infinity"] =Value::number(std::numeric_limits<double>::infinity());
        defines["newline"]  =Value::string("\n");
        defines["tab"]      =Value::string("\t");
        defines["version"]  =Value::string("EL1 v2.0");
    }

    // ── run a set of lines ──────────────────────────────────
    void runLines(const std::vector<std::string>& lines, ScopeRef scope, size_t start=0, size_t end=0){
        if(end==0) end=lines.size();
        size_t i=start;
        while(i<end){
            try { i=executeLine(lines,i,end,scope); }
            catch(ReturnSignal&)   { throw; }
            catch(BreakSignal&)    { throw; }
            catch(ContinueSignal&) { throw; }
            catch(ExitSignal&)     { throw; }
            catch(ThrowSignal&)    { throw; }
            catch(std::exception& ex){
                // wrap as ThrowSignal so user try/catch blocks can catch it
                throw ThrowSignal{ex.what(), Value::string(ex.what())};
            }
        }
    }

    // ── main dispatch ───────────────────────────────────────
    size_t executeLine(const std::vector<std::string>& lines, size_t i, size_t end, ScopeRef scope){
        std::string raw=lines[i];
        std::string line=trim(raw);
        if(line.empty()||line[0]=='#'||line.substr(0,2)=="//") return i+1;

        std::string lo=toLower(line);
        auto words=splitWords(lo);
        if(words.empty()) return i+1;
        auto origWords=splitWords(line);

        // ══ DEFINES ═══════════════════════════════════════════
        if(words[0]=="define"&&words.size()>=4&&words[2]=="as"){
            defines[toLower(words[1])]=evalExpr(trim(line.substr(line.find(" as ")+4)),scope);
            return i+1;
        }

        // ══ VARIABLES ═════════════════════════════════════════
        if(words[0]=="let"&&words.size()>=4&&words[2]=="be"){
            bool isSpecial=(words.size()>=5&&(words[3]=="math"||words[3]=="length"||words[3]=="type"||words[3]=="string"||words[3]=="number"||words[3]=="json"||words[3]=="lines"||words[3]=="words"||words[3]=="upper"||words[3]=="lower"||words[3]=="trim"||words[3]=="format"||words[3]=="input"||words[3]=="reversed"||words[3]=="sorted"||words[3]=="keys"||words[3]=="values"||words[3]=="joined"||words[3]=="split"||words[3]=="env"||words[3]=="output"||words[3]=="index"||words[3]=="count"||words[3]=="first"||words[3]=="last"||words[3]=="random"));
            if(!isSpecial){
                std::string varName=words[1];
                scope->define(varName,evalExpr(trim(line.substr(line.find(" be ")+4)),scope));
                return i+1;
            }
        }

        // ── let x be math ──────────────────────────────────
        if(words[0]=="let"&&words.size()>=5&&words[2]=="be"&&words[3]=="math"){
            return handleLetMath(words,origWords,line,scope,i);
        }
        // ── let x be length of y ────────────────────────────
        if(words[0]=="let"&&words.size()>=6&&words[2]=="be"&&words[3]=="length"&&words[4]=="of"){
            // collect full expression after "of"
            std::string exprStr;
            for(size_t xi=5;xi<origWords.size();xi++) exprStr+=origWords[xi]+(xi+1<origWords.size()?" ":"");
            auto v=evalExpr(trim(exprStr),scope);
            double len=0;
            if(v->type==VType::String) len=(double)v->s.size();
            else if(v->type==VType::Table) len=(double)(v->arr.size()+v->tbl.size());
            scope->define(words[1],Value::number(len));
            return i+1;
        }
        // ── let x be type of y ──────────────────────────────
        if(words[0]=="let"&&words.size()>=6&&words[2]=="be"&&words[3]=="type"&&words[4]=="of"){
            auto v=evalExpr(origWords[5],scope);
            std::string t;
            switch(v->type){
                case VType::Nil: t="nil"; break; case VType::Bool: t="bool"; break;
                case VType::Number: t="number"; break; case VType::String: t="string"; break;
                case VType::Table: t="table"; break; case VType::Function: t="function"; break;
            }
            scope->define(words[1],Value::string(t));
            return i+1;
        }
        // ── let x be string of y ───────────────────────────
        if(words[0]=="let"&&words.size()>=6&&words[2]=="be"&&words[3]=="string"&&words[4]=="of"){
            scope->define(words[1],Value::string(evalExpr(origWords[5],scope)->repr()));
            return i+1;
        }
        // ── let x be number of y ───────────────────────────
        if(words[0]=="let"&&words.size()>=6&&words[2]=="be"&&words[3]=="number"&&words[4]=="of"){
            auto v=evalExpr(origWords[5],scope);
            double n=0;
            if(v->type==VType::Number) n=v->n;
            else if(isNumber(v->s)) n=std::stod(v->s);
            scope->define(words[1],Value::number(n));
            return i+1;
        }
        // ── let x be upper of y ───────────────────────────
        if(words[0]=="let"&&words.size()>=6&&words[2]=="be"&&words[3]=="upper"&&words[4]=="of"){
            scope->define(words[1],Value::string(toUpper(evalExpr(origWords[5],scope)->s)));
            return i+1;
        }
        // ── let x be lower of y ───────────────────────────
        if(words[0]=="let"&&words.size()>=6&&words[2]=="be"&&words[3]=="lower"&&words[4]=="of"){
            scope->define(words[1],Value::string(toLower(evalExpr(origWords[5],scope)->s)));
            return i+1;
        }
        // ── let x be trim of y ────────────────────────────
        if(words[0]=="let"&&words.size()>=6&&words[2]=="be"&&words[3]=="trim"&&words[4]=="of"){
            scope->define(words[1],Value::string(trim(evalExpr(origWords[5],scope)->s)));
            return i+1;
        }
        // ── let x be json of y ────────────────────────────
        if(words[0]=="let"&&words.size()>=6&&words[2]=="be"&&words[3]=="json"&&words[4]=="of"){
            scope->define(words[1],Value::string(encodeJson(evalExpr(origWords[5],scope))));
            return i+1;
        }
        // ── let x be reversed of y ───────────────────────
        if(words[0]=="let"&&words.size()>=6&&words[2]=="be"&&words[3]=="reversed"&&words[4]=="of"){
            auto v=evalExpr(origWords[5],scope);
            auto result=Value::table();
            if(v->type==VType::String){
                std::string s=v->s; std::reverse(s.begin(),s.end());
                scope->define(words[1],Value::string(s));
            } else if(v->type==VType::Table){
                result->arr=v->arr; std::reverse(result->arr.begin(),result->arr.end());
                scope->define(words[1],result);
            }
            return i+1;
        }
        // ── let x be sorted of y ─────────────────────────
        if(words[0]=="let"&&words.size()>=6&&words[2]=="be"&&words[3]=="sorted"&&words[4]=="of"){
            auto v=evalExpr(origWords[5],scope);
            auto result=Value::table();
            result->arr=v->arr;
            std::sort(result->arr.begin(),result->arr.end(),[](const ValueRef& a,const ValueRef& b){
                if(a->type==VType::Number&&b->type==VType::Number) return a->n<b->n;
                return a->repr()<b->repr();
            });
            scope->define(words[1],result);
            return i+1;
        }
        // ── let x be keys of y ───────────────────────────
        if(words[0]=="let"&&words.size()>=6&&words[2]=="be"&&words[3]=="keys"&&words[4]=="of"){
            auto v=evalExpr(origWords[5],scope);
            auto result=Value::table();
            for(auto& [k,_]:v->tbl) result->arr.push_back(Value::string(k));
            scope->define(words[1],result);
            return i+1;
        }
        // ── let x be values of y ─────────────────────────
        if(words[0]=="let"&&words.size()>=6&&words[2]=="be"&&words[3]=="values"&&words[4]=="of"){
            auto v=evalExpr(origWords[5],scope);
            auto result=Value::table();
            for(auto& [_,val]:v->tbl) result->arr.push_back(val);
            scope->define(words[1],result);
            return i+1;
        }
        // ── let x be first of y ──────────────────────────
        if(words[0]=="let"&&words.size()>=6&&words[2]=="be"&&words[3]=="first"&&words[4]=="of"){
            auto v=evalExpr(origWords[5],scope);
            if(v->type==VType::Table&&!v->arr.empty()) scope->define(words[1],v->arr[0]);
            else if(v->type==VType::String&&!v->s.empty()) scope->define(words[1],Value::string(std::string(1,v->s[0])));
            else scope->define(words[1],Value::nil());
            return i+1;
        }
        // ── let x be last of y ───────────────────────────
        if(words[0]=="let"&&words.size()>=6&&words[2]=="be"&&words[3]=="last"&&words[4]=="of"){
            auto v=evalExpr(origWords[5],scope);
            if(v->type==VType::Table&&!v->arr.empty()) scope->define(words[1],v->arr.back());
            else if(v->type==VType::String&&!v->s.empty()) scope->define(words[1],Value::string(std::string(1,v->s.back())));
            else scope->define(words[1],Value::nil());
            return i+1;
        }
        // ── let x be count of needle in haystack ────────
        if(words[0]=="let"&&words.size()>=7&&words[2]=="be"&&words[3]=="count"&&words[4]=="of"){
            size_t inPos=std::find(words.begin(),words.end(),"in")-words.begin();
            if(inPos<words.size()){
                auto needle=evalExpr(origWords[5],scope);
                auto haystack=evalExpr(origWords[inPos+1],scope);
                int cnt=0;
                if(haystack->type==VType::Table){
                    for(auto& v:haystack->arr) if(*v==*needle) cnt++;
                }
                scope->define(words[1],Value::number(cnt));
            }
            return i+1;
        }
        // ── let x be index of needle in haystack ────────
        if(words[0]=="let"&&words.size()>=7&&words[2]=="be"&&words[3]=="index"&&words[4]=="of"){
            size_t inPos=std::find(words.begin(),words.end(),"in")-words.begin();
            if(inPos<words.size()){
                auto needle=evalExpr(origWords[5],scope);
                auto haystack=evalExpr(origWords[inPos+1],scope);
                int idx2=-1;
                if(haystack->type==VType::Table){
                    for(size_t ii=0;ii<haystack->arr.size();ii++) if(*haystack->arr[ii]==*needle){idx2=(int)ii;break;}
                } else if(haystack->type==VType::String){
                    size_t p=haystack->s.find(needle->s);
                    if(p!=std::string::npos) idx2=(int)p;
                }
                scope->define(words[1],Value::number(idx2));
            }
            return i+1;
        }
        // ── let x be lines of y ──────────────────────────
        if(words[0]=="let"&&words.size()>=6&&words[2]=="be"&&words[3]=="lines"&&words[4]=="of"){
            auto v=evalExpr(origWords[5],scope);
            auto result=Value::table();
            auto parts=strSplit(v->s,"\n");
            for(auto& p:parts) result->arr.push_back(Value::string(p));
            scope->define(words[1],result);
            return i+1;
        }
        // ── let x be words of y ──────────────────────────
        if(words[0]=="let"&&words.size()>=6&&words[2]=="be"&&words[3]=="words"&&words[4]=="of"){
            auto v=evalExpr(origWords[5],scope);
            auto result=Value::table();
            auto ws=splitWords(v->s);
            for(auto& w:ws) result->arr.push_back(Value::string(w));
            scope->define(words[1],result);
            return i+1;
        }
        // ── let x be env of "VAR" ────────────────────────
        if(words[0]=="let"&&words.size()>=6&&words[2]=="be"&&words[3]=="env"&&words[4]=="of"){
            std::string envVar=evalExpr(origWords[5],scope)->s;
            const char* ev=std::getenv(envVar.c_str());
            scope->define(words[1],ev?Value::string(ev):Value::nil());
            return i+1;
        }
        // ── let x be output of "cmd" ─────────────────────
        if(words[0]=="let"&&words.size()>=6&&words[2]=="be"&&words[3]=="output"&&words[4]=="of"){
            std::string cmd=evalExpr(origWords[5],scope)->s;
            std::string result2;
            FILE* p=popen(cmd.c_str(),"r");
            if(p){
                char buf[256]; while(fgets(buf,sizeof(buf),p)) result2+=buf;
                pclose(p);
            }
            scope->define(words[1],Value::string(trim(result2)));
            return i+1;
        }
        // ── let x be split of str by delim ───────────────
        if(words[0]=="let"&&words.size()>=8&&words[2]=="be"&&words[3]=="split"){
            size_t byPos=std::find(words.begin(),words.end(),"by")-words.begin();
            if(byPos<words.size()){
                auto haystack=evalExpr(origWords[5],scope);
                auto delim=evalExpr(origWords[byPos+1],scope);
                auto result=Value::table();
                for(auto& p:strSplit(haystack->s,delim->s)) result->arr.push_back(Value::string(p));
                scope->define(words[1],result);
            }
            return i+1;
        }
        // ── let x be joined of tbl by delim ─────────────
        if(words[0]=="let"&&words.size()>=8&&words[2]=="be"&&words[3]=="joined"){
            size_t byPos=std::find(words.begin(),words.end(),"by")-words.begin();
            if(byPos<words.size()){
                auto tblV=evalExpr(origWords[5],scope);
                auto delim=evalExpr(origWords[byPos+1],scope);
                std::string joined;
                for(size_t ii=0;ii<tblV->arr.size();ii++){
                    if(ii) joined+=delim->s;
                    joined+=tblV->arr[ii]->repr();
                }
                scope->define(words[1],Value::string(joined));
            }
            return i+1;
        }
        // ── let x be random int from a to b ─────────────
        if(words[0]=="let"&&words.size()>=8&&words[2]=="be"&&words[3]=="random"&&words[4]=="int"){
            size_t fromP=std::find(words.begin(),words.end(),"from")-words.begin();
            size_t toP  =std::find(words.begin(),words.end(),"to")  -words.begin();
            if(fromP<words.size()&&toP<words.size()){
                int lo2=(int)evalExpr(origWords[fromP+1],scope)->n;
                int hi2=(int)evalExpr(origWords[toP+1],  scope)->n;
                scope->define(words[1],Value::number(lo2+rand()%(hi2-lo2+1)));
            }
            return i+1;
        }
        // ── let x be format "template" with vals ────────
        if(words[0]=="let"&&words.size()>=6&&words[2]=="be"&&words[3]=="format"){
            size_t withP=lo.find(" with ");
            if(withP!=std::string::npos){
                std::string tmpl=evalExpr(trim(line.substr(line.find("format ")+7,withP-(line.find("format ")+7))),scope)->s;
                std::string argStr=trim(line.substr(withP+6));
                auto args=parseArgList(argStr,scope);
                // replace {} placeholders in order
                for(auto& arg:args){
                    size_t ph=tmpl.find("{}");
                    if(ph!=std::string::npos) tmpl.replace(ph,2,arg->repr());
                }
                scope->define(words[1],Value::string(tmpl));
            }
            return i+1;
        }
        // ── let x be input from user ─────────────────────
        if(words[0]=="let"&&lo.find(" be input")!=std::string::npos){
            size_t promptP=lo.find("prompt ");
            std::string prompt="";
            if(promptP!=std::string::npos) prompt=evalExpr(trim(line.substr(promptP+7)),scope)->s;
            if(!noConsole){
                std::cout<<prompt;
                std::string inp; std::getline(std::cin,inp);
                if(isNumber(inp)) scope->define(words[1],Value::number(std::stod(inp)));
                else scope->define(words[1],Value::string(inp));
            } else scope->define(words[1],Value::string(""));
            return i+1;
        }

        // ══ SET ═══════════════════════════════════════════════
        if(words[0]=="set"&&words.size()>=4&&words[2]=="to"){
            std::string varName=words[1];
            // handle set tbl[key] to val
            if(varName.find('[')!=std::string::npos){
                size_t lb=varName.find('['), rb=varName.find(']');
                std::string tblName=varName.substr(0,lb);
                std::string keyStr=varName.substr(lb+1,rb-lb-1);
                auto tbl=scope->get(tblName);
                auto keyV=evalExpr(keyStr,scope);
                auto val=evalExpr(trim(line.substr(line.find(" to ")+4)),scope);
                if(tbl->type==VType::Table){
                    if(keyV->type==VType::Number) tbl->arr[(size_t)keyV->n]=val;
                    else tbl->tbl[keyV->s]=val;
                }
            } else {
                scope->set(varName,evalExpr(trim(line.substr(line.find(" to ")+4)),scope));
            }
            return i+1;
        }
        if(words[0]=="change"&&words.size()>=4&&words[2]=="by"){
            auto cur=scope->get(words[1]);
            auto dv=evalExpr(words[3],scope);
            scope->set(words[1],Value::number(cur->n+dv->n));
            return i+1;
        }
        if((words[0]=="increase"||words[0]=="decrease")&&words.size()>=4&&words[2]=="by"){
            auto cur=scope->get(words[1]);
            auto dv=evalExpr(words[3],scope);
            double res=words[0]=="increase"?cur->n+dv->n:cur->n-dv->n;
            scope->set(words[1],Value::number(res));
            return i+1;
        }

        // ══ SAY / PRINT ═══════════════════════════════════════
        if(words[0]=="say"&&!noConsole){
            size_t sayPos=lo.find("say ");
            std::string rest=(sayPos!=std::string::npos)?line.substr(sayPos+4):line.substr(4);
            std::cout<<evalExpr(trim(rest),scope)->repr()<<"\n";
            return i+1;
        }
        if(words[0]=="say") return i+1;

        // ── say same line (no newline) ───────────────────────
        if(words[0]=="write"&&!noConsole){
            size_t p2=lo.find("write ");
            std::string rest=(p2!=std::string::npos)?line.substr(p2+6):line.substr(6);
            std::cout<<evalExpr(trim(rest),scope)->repr()<<std::flush;
            return i+1;
        }
        if(words[0]=="write") return i+1;

        // ── log levels ──────────────────────────────────────
        if(words[0]=="log"){
            if(!noConsole){
                std::string level="INFO";
                size_t msgStart=4;
                if(words.size()>=3&&words[1]=="warn")  {level="WARN";  msgStart=lo.find("warn ")+5;}
                if(words.size()>=3&&words[1]=="error") {level="ERROR"; msgStart=lo.find("error ")+6;}
                auto now=std::chrono::system_clock::now();
                auto t=std::chrono::system_clock::to_time_t(now);
                struct tm* tm_info=localtime(&t);
                char tbuf[20]; strftime(tbuf,sizeof(tbuf),"%H:%M:%S",tm_info);
                std::string msg=evalExpr(trim(line.substr(msgStart)),scope)->repr();
                if(level=="ERROR") std::cerr<<"["<<tbuf<<"] ["<<level<<"] "<<msg<<"\n";
                else               std::cout<<"["<<tbuf<<"] ["<<level<<"] "<<msg<<"\n";
            }
            return i+1;
        }

        // ══ INPUT / ASK ═══════════════════════════════════════
        if(words[0]=="ask"&&!noConsole){
            size_t intoPos=lo.find(" into ");
            std::string prompt=line.substr(3,intoPos-3+3);
            if(intoPos!=std::string::npos){
                std::string varName=trim(line.substr(intoPos+6));
                std::cout<<evalExpr(trim(prompt),scope)->repr();
                std::string inp; std::getline(std::cin,inp);
                if(isNumber(inp)) scope->set(varName,Value::number(std::stod(inp)));
                else scope->set(varName,Value::string(inp));
            }
            return i+1;
        }
        if(words[0]=="ask") return i+1;

        // ══ FILE I/O ══════════════════════════════════════════
        // read file "path" into x
        if(words[0]=="read"&&words[1]=="file"){
            size_t intoP=lo.find(" into ");
            std::string pathExpr=trim(line.substr(lo.find("file ")+5, intoP-(lo.find("file ")+5)));
            std::string path=evalExpr(pathExpr,scope)->s;
            std::string varName=intoP!=std::string::npos?trim(line.substr(intoP+6)):"__filedata";
            std::ifstream f(path);
            if(!f) throw std::runtime_error("Cannot open file: "+path);
            std::ostringstream ss; ss<<f.rdbuf();
            scope->set(varName,Value::string(ss.str()));
            return i+1;
        }
        // write file "path" with content
        if(words[0]=="write"&&words[1]=="file"){
            size_t withP=lo.find(" with ");
            std::string pathExpr=trim(line.substr(lo.find("file ")+5, withP-(lo.find("file ")+5)));
            std::string path=evalExpr(pathExpr,scope)->s;
            std::string content=withP!=std::string::npos?evalExpr(trim(line.substr(withP+6)),scope)->s:"";
            std::ofstream f(path);
            if(!f) throw std::runtime_error("Cannot write file: "+path);
            f<<content;
            return i+1;
        }
        // append to file "path" with content
        if(words[0]=="append"&&words[1]=="to"&&words[2]=="file"){
            size_t withP=lo.find(" with ");
            std::string pathExpr=trim(line.substr(lo.find("file ")+5, withP-(lo.find("file ")+5)));
            std::string path=evalExpr(pathExpr,scope)->s;
            std::string content=withP!=std::string::npos?evalExpr(trim(line.substr(withP+6)),scope)->s:"";
            std::ofstream f(path,std::ios::app);
            if(!f) throw std::runtime_error("Cannot append to file: "+path);
            f<<content;
            return i+1;
        }
        // delete file "path"
        if(words[0]=="delete"&&words[1]=="file"){
            std::string path=evalExpr(trim(line.substr(lo.find("file ")+5)),scope)->s;
            std::remove(path.c_str());
            return i+1;
        }
        // file exists "path" into x
        if(words[0]=="file"&&words[1]=="exists"){
            size_t intoP=lo.find(" into ");
            std::string pathExpr=trim(line.substr(lo.find("exists ")+7,intoP-(lo.find("exists ")+7)));
            std::string varName=intoP!=std::string::npos?trim(line.substr(intoP+6)):"__exists";
            std::string path=evalExpr(pathExpr,scope)->s;
            struct stat st; bool ex=(stat(path.c_str(),&st)==0);
            scope->set(varName,Value::boolean(ex));
            return i+1;
        }
        // list files in "dir" into x
        if(words[0]=="list"&&words[1]=="files"&&words[2]=="in"){
            size_t intoP=lo.find(" into ");
            std::string dirExpr=trim(line.substr(lo.find(" in ")+4,intoP-(lo.find(" in ")+4)));
            std::string dirPath=evalExpr(dirExpr,scope)->s;
            std::string varName=intoP!=std::string::npos?trim(line.substr(intoP+6)):"__files";
            auto result=Value::table();
            #ifdef _WIN32
            WIN32_FIND_DATAA fd; std::string pat=dirPath+"\\*";
            HANDLE h=FindFirstFileA(pat.c_str(),&fd);
            if(h!=INVALID_HANDLE_VALUE){ do{ result->arr.push_back(Value::string(fd.cFileName)); }while(FindNextFileA(h,&fd)); FindClose(h); }
            #else
            DIR* d=opendir(dirPath.c_str());
            if(d){ struct dirent* ent; while((ent=readdir(d))!=nullptr) result->arr.push_back(Value::string(ent->d_name)); closedir(d); }
            #endif
            scope->set(varName,result);
            return i+1;
        }

        // ══ STRING OPS ════════════════════════════════════════
        // replace "from" with "to" in varname
        if(words[0]=="replace"){
            size_t withP=lo.find(" with "), inP=lo.rfind(" in ");
            if(withP!=std::string::npos&&inP!=std::string::npos){
                auto fromV=evalExpr(trim(line.substr(8,withP-8)),scope);
                auto toV  =evalExpr(trim(line.substr(withP+6,inP-(withP+6))),scope);
                std::string varName=trim(line.substr(inP+4));
                auto sv=scope->get(varName);
                scope->set(varName,Value::string(strReplace(sv->s,fromV->s,toV->s)));
            }
            return i+1;
        }

        // ══ TABLE OPS ═════════════════════════════════════════
        if(words[0]=="create"&&words[1]=="table"&&words.size()>=3){
            scope->define(words[2],Value::table()); return i+1;
        }
        if(words[0]=="add"&&words.size()>=4){
            size_t toIdx=std::find(words.begin(),words.end(),"to")-words.begin();
            if(toIdx<words.size()){
                std::string tblName=origWords[toIdx+1];
                auto tbl=scope->get(tblName);
                if(tbl->type==VType::Table){
                    std::string rawLine=trim(line.substr(4));
                    size_t arrowPos=rawLine.find("->");
                    if(arrowPos!=std::string::npos){
                        size_t t2=rawLine.find(" to "); if(t2==std::string::npos) t2=rawLine.size();
                        auto k=evalExpr(trim(rawLine.substr(0,arrowPos)),scope);
                        auto v=evalExpr(trim(rawLine.substr(arrowPos+2,t2-(arrowPos+2))),scope);
                        tbl->tbl[k->repr()]=v;
                    } else {
                        size_t t2=rawLine.find(" to "); if(t2==std::string::npos) t2=rawLine.size();
                        tbl->arr.push_back(evalExpr(trim(rawLine.substr(0,t2)),scope));
                    }
                }
            }
            return i+1;
        }
        if(words[0]=="remove"&&words.size()>=4&&words[2]=="from"){
            auto tbl=scope->get(origWords[3]);
            if(tbl->type==VType::Table){
                auto idx=evalExpr(words[1],scope);
                if(idx->type==VType::Number){
                    size_t ii=(size_t)idx->n;
                    if(ii<tbl->arr.size()) tbl->arr.erase(tbl->arr.begin()+ii);
                } else tbl->tbl.erase(idx->s);
            }
            return i+1;
        }
        // sort table x
        if(words[0]=="sort"&&words[1]=="table"&&words.size()>=3){
            auto tbl=scope->get(words[2]);
            if(tbl->type==VType::Table){
                std::sort(tbl->arr.begin(),tbl->arr.end(),[](const ValueRef& a,const ValueRef& b){
                    if(a->type==VType::Number&&b->type==VType::Number) return a->n<b->n;
                    return a->repr()<b->repr();
                });
            }
            return i+1;
        }
        // reverse table x
        if(words[0]=="reverse"&&words[1]=="table"&&words.size()>=3){
            auto tbl=scope->get(words[2]);
            if(tbl->type==VType::Table) std::reverse(tbl->arr.begin(),tbl->arr.end());
            return i+1;
        }
        // clear table x
        if(words[0]=="clear"&&words[1]=="table"&&words.size()>=3){
            auto tbl=scope->get(words[2]);
            if(tbl->type==VType::Table){ tbl->arr.clear(); tbl->tbl.clear(); }
            return i+1;
        }
        // merge table x into y
        if(words[0]=="merge"&&words[1]=="table"&&words.size()>=5&&words[3]=="into"){
            auto src=scope->get(words[2]);
            auto dst=scope->get(words[4]);
            if(src->type==VType::Table&&dst->type==VType::Table){
                for(auto& v:src->arr) dst->arr.push_back(v);
                for(auto& [k,v]:src->tbl) dst->tbl[k]=v;
            }
            return i+1;
        }

        // ══ JSON DECODE ══════════════════════════════════════
        // decode json str into x
        if(words[0]=="decode"&&words[1]=="json"){
            size_t intoP=lo.find(" into ");
            std::string jsonExpr=trim(line.substr(lo.find("json ")+5,intoP-(lo.find("json ")+5)));
            std::string varName=intoP!=std::string::npos?trim(line.substr(intoP+6)):"__json";
            std::string jsonStr=evalExpr(jsonExpr,scope)->s;
            size_t pos2=0;
            scope->set(varName,decodeJson(jsonStr,pos2));
            return i+1;
        }

        // ══ HTTP ══════════════════════════════════════════════
        // fetch url "https://..." into x
        if(words[0]=="fetch"&&words[1]=="url"){
            size_t intoP=lo.find(" into ");
            std::string urlExpr=trim(line.substr(lo.find("url ")+4,intoP-(lo.find("url ")+4)));
            std::string url=evalExpr(urlExpr,scope)->s;
            std::string varName=intoP!=std::string::npos?trim(line.substr(intoP+6)):"__http";
            // Use curl subprocess (universally available)
            std::string cmd="curl -s --max-time 10 \""+url+"\"";
            std::string body;
            FILE* pp=popen(cmd.c_str(),"r");
            if(pp){ char buf[4096]; while(fgets(buf,sizeof(buf),pp)) body+=buf; pclose(pp); }
            scope->set(varName,Value::string(trim(body)));
            return i+1;
        }
        // post to url "https://..." with data into x
        if(words[0]=="post"&&words[1]=="to"&&words[2]=="url"){
            size_t withP=lo.find(" with "), intoP=lo.find(" into ");
            std::string urlExpr=trim(line.substr(lo.find("url ")+4,withP-(lo.find("url ")+4)));
            std::string url=evalExpr(urlExpr,scope)->s;
            std::string dataExpr=withP!=std::string::npos?trim(line.substr(withP+6,intoP-(withP+6))):"";
            std::string data=evalExpr(dataExpr,scope)->s;
            std::string varName=intoP!=std::string::npos?trim(line.substr(intoP+6)):"__http";
            std::string cmd="curl -s -X POST -d '"+data+"' \""+url+"\"";
            std::string body;
            FILE* pp=popen(cmd.c_str(),"r");
            if(pp){ char buf[4096]; while(fgets(buf,sizeof(buf),pp)) body+=buf; pclose(pp); }
            scope->set(varName,Value::string(trim(body)));
            return i+1;
        }

        // ══ ERROR HANDLING ════════════════════════════════════
        // try do ... catch error as e do ... done
        if(words[0]=="try"&&(words.size()<2||words[1]=="do")){
            return handleTryCatch(lines,i,end,scope);
        }
        // throw error "message"
        if(words[0]=="throw"){
            size_t msgStart=lo.find("error ")+6;
            if(lo.find("error ")==std::string::npos) msgStart=6;
            std::string msg=evalExpr(trim(line.substr(msgStart)),scope)->repr();
            throw ThrowSignal{msg,Value::string(msg)};
        }
        // assert x is 5 or fail "msg"
        if(words[0]=="assert"){
            size_t orFailP=lo.find(" or fail ");
            std::string condStr=trim(line.substr(7,orFailP==std::string::npos?std::string::npos:orFailP-7));
            std::string msg="Assertion failed";
            if(orFailP!=std::string::npos) msg=evalExpr(trim(line.substr(orFailP+9)),scope)->s;
            bool cond=evalEnglishCondition(condStr,scope);
            if(!cond) throw ThrowSignal{msg,Value::string(msg)};
            return i+1;
        }

        // ══ FUNCTIONS ═════════════════════════════════════════
        if((words[0]=="make"&&words.size()>1&&words[1]=="function")||(words[0]=="function")){
            return handleFunctionDef(lines,i,end,scope,words,origWords,lo);
        }
        if(words[0]=="return"||(words.size()>=2&&words[0]=="give"&&words[1]=="back")){
            std::string rest=words[0]=="return"?line.substr(6):line.substr(9);
            throw ReturnSignal{evalExpr(trim(rest),scope)};
        }
        if(words[0]=="run"||words[0]=="call"){
            std::string fnName=origWords[1];
            std::vector<ValueRef> args;
            size_t withP=lo.find(" with ");
            if(withP!=std::string::npos) args=parseArgList(trim(line.substr(withP+6)),scope);
            callFunction(fnName,args,scope);
            return i+1;
        }

        // ══ EVENTS ════════════════════════════════════════════
        // on event "eventname" do ... done
        if(words[0]=="on"&&words[1]=="event"){
            std::string evName=evalExpr(origWords[2],scope)->s;
            size_t bi=i+1; int depth=1;
            std::vector<std::string> body;
            while(bi<end&&depth>0){
                std::string blo=toLower(trim(lines[bi]));
                auto bw=splitWords(blo);
                if(!bw.empty()){if(isBlockStart(bw)) depth++; if(bw[0]=="done"||bw[0]=="end") depth--;}
                if(depth>0) body.push_back(lines[bi]); bi++;
            }
            eventHandlers[evName].push_back(body);
            return bi;
        }
        // trigger event "eventname"
        if(words[0]=="trigger"&&words[1]=="event"){
            std::string evName=evalExpr(origWords[2],scope)->s;
            auto it=eventHandlers.find(evName);
            if(it!=eventHandlers.end()){
                for(auto& body:it->second){
                    auto es=std::make_shared<Scope>(scope);
                    try{ runLines(body,es); } catch(BreakSignal&){} 
                }
            }
            return i+1;
        }

        // ══ LOOPS ═════════════════════════════════════════════
        if(words[0]=="repeat") return handleRepeat(lines,i,end,scope,words,origWords);
        if(words[0]=="loop"&&words.size()>1&&words[1]=="while") return handleLoopWhile(lines,i,end,scope,lo,line);
        if(words[0]=="for"&&words.size()>1&&words[1]=="each")   return handleForEach(lines,i,end,scope,words,origWords);
        // for x from 1 to 10 do
        if(words[0]=="for"&&words.size()>=6&&words[2]=="from"){
            return handleForRange(lines,i,end,scope,words,origWords);
        }
        if(words[0]=="stop")     throw BreakSignal{};
        if(words[0]=="skip")     throw ContinueSignal{};

        // ══ CONDITIONALS ══════════════════════════════════════
        if(words[0]=="however"&&words.size()>1&&words[1]=="if") return handleHoweverIf(lines,i,end,scope,lo,line);
        if(words[0]=="also"&&words.size()>1&&words[1]=="if")    return skipBlock(lines,i+1,end);
        if(words[0]=="otherwise") return skipBlock(lines,i+1,end);

        // ══ MATCH / SWITCH ════════════════════════════════════
        // match x with ... done
        if(words[0]=="match") return handleMatch(lines,i,end,scope,words,origWords,lo,line);

        // ══ IMPORT ════════════════════════════════════════════
        // import "other.el"
        if(words[0]=="import"){
            std::string path=evalExpr(trim(line.substr(7)),scope)->s;
            if(!scriptDir.empty()) path=scriptDir+"/"+path;
            if(importedFiles.find(path)==importedFiles.end()){
                importedFiles.insert(path);
                runFile(path,scope);
            }
            return i+1;
        }
        // export name  (just documents intent; variable already in scope)
        if(words[0]=="export") return i+1;

        // ══ PROCESS ═══════════════════════════════════════════
        // run command "cmd"
        if(words[0]=="run"&&words[1]=="command"){
            std::string cmd=evalExpr(trim(line.substr(lo.find("command ")+8)),scope)->s;
            std::system(cmd.c_str());
            return i+1;
        }

        // ══ GUI WINDOW ════════════════════════════════════════
        if(words[0]=="open"&&words[1]=="window") return handleGuiOpen(words,origWords,lo,line,scope,i);
        if(words[0]=="add"&&(words[1]=="button"||words[1]=="label"||words[1]=="input"||words[1]=="title"||words[1]=="text")) return handleGuiAdd(words,origWords,lo,line,scope,i);
        if(words[0]=="show"&&words[1]=="window") return handleGuiShow(words,scope,i);
        if(words[0]=="close"&&words[1]=="window") return handleGuiClose(words,scope,i);
        if(words[0]=="set"&&words[2]=="text"&&words[3]=="to") return handleGuiSetText(words,origWords,lo,line,scope,i);

        // ══ MISC ══════════════════════════════════════════════
        if(words[0]=="sleep"){
            double ms=evalExpr(words[1],scope)->n;
            if(words.size()>=3&&(words[2]=="seconds"||words[2]=="second")) ms*=1000;
            std::this_thread::sleep_for(std::chrono::milliseconds((long long)ms));
            return i+1;
        }
        if(words[0]=="exit"){
            int code=0;
            if(words.size()>=3&&words[1]=="with") code=(int)evalExpr(words[2],scope)->n;
            throw ExitSignal{code};
        }
        if(words[0]=="done"||words[0]=="end") return i+1;

        // ── fallback: evaluate as expression (bare fn call etc) ──
        try { evalExpr(line,scope); } catch(...) {}
        return i+1;
    }

    // ─── LET MATH ──────────────────────────────────────────
    size_t handleLetMath(const std::vector<std::string>& words, const std::vector<std::string>& origWords, const std::string& line, ScopeRef scope, size_t i){
        std::string varName=words[1];
        std::string fn=words[4];
        auto origW=splitWords(line);
        // collect the argument (everything from index 6 onward, handling "of" at 5)
        std::string argStr;
        size_t argStart=origW.size()>=7?6:5;
        for(size_t x=argStart;x<origW.size();x++) argStr+=origW[x]+(x+1<origW.size()?" ":"");
        auto v=evalExpr(trim(argStr),scope);
        double res=0;
        if(fn=="sqrt")        res=std::sqrt(v->n);
        else if(fn=="abs")    res=std::abs(v->n);
        else if(fn=="floor")  res=std::floor(v->n);
        else if(fn=="ceil")   res=std::ceil(v->n);
        else if(fn=="round")  res=std::round(v->n);
        else if(fn=="sin")    res=std::sin(v->n);
        else if(fn=="cos")    res=std::cos(v->n);
        else if(fn=="tan")    res=std::tan(v->n);
        else if(fn=="asin")   res=std::asin(v->n);
        else if(fn=="acos")   res=std::acos(v->n);
        else if(fn=="atan")   res=std::atan(v->n);
        else if(fn=="log")    res=std::log(v->n);
        else if(fn=="log2")   res=std::log2(v->n);
        else if(fn=="log10")  res=std::log10(v->n);
        else if(fn=="exp")    res=std::exp(v->n);
        else if(fn=="sign")   res=v->n>0?1:v->n<0?-1:0;
        else if(fn=="power"){
            // math power of base exp N   OR   math power of base N
            double base=v->n;
            double exp2=2;
            // find the exponent: skip "exp" keyword if present
            size_t expIdx=7;
            if(origW.size()>7 && toLower(origW[7])=="exp") expIdx=8;
            if(origW.size()>expIdx) exp2=evalExpr(origW[expIdx],scope)->n;
            res=std::pow(base,exp2);
        }
        else if(fn=="clamp"){
            // math clamp of x between min and max
            size_t betP=std::find(words.begin(),words.end(),"between")-words.begin();
            size_t andP=std::find(words.begin(),words.end(),"and")-words.begin();
            if(betP<words.size()&&andP<words.size()){
                double mn=evalExpr(origW[betP+1],scope)->n;
                double mx=evalExpr(origW[andP+1],scope)->n;
                res=std::max(mn,std::min(mx,v->n));
            } else res=v->n;
        }
        else if(fn=="lerp"){
            // math lerp of a to b by t
            size_t toP =std::find(words.begin(),words.end(),"to") -words.begin();
            size_t byP2=std::find(words.begin(),words.end(),"by") -words.begin();
            if(toP<words.size()&&byP2<words.size()){
                double b2=evalExpr(origW[toP+1],scope)->n;
                double t2=evalExpr(origW[byP2+1],scope)->n;
                res=v->n+(b2-v->n)*t2;
            } else res=v->n;
        }
        scope->define(varName,Value::number(res));
        return i+1;
    }

    // ─── FUNCTION DEFINITION ───────────────────────────────
    size_t handleFunctionDef(const std::vector<std::string>& lines, size_t i, size_t end, ScopeRef scope,
        const std::vector<std::string>& words, const std::vector<std::string>& origWords, const std::string& lo)
    {
        size_t nameIdx=words[0]=="make"?2:1;
        std::string fnName=origWords[nameIdx];
        std::vector<std::string> params;
        size_t withIdx=std::find(words.begin(),words.end(),"with")-words.begin();
        size_t doIdx  =std::find(words.begin(),words.end(),"do")  -words.begin();
        if(withIdx<doIdx){
            for(size_t p=withIdx+1;p<doIdx;p++){
                std::string pw=origWords[p];
                if(pw!="and"&&pw!=","&&!pw.empty()) params.push_back(pw);
            }
        }
        int depth=1; size_t bi=i+1;
        std::vector<std::string> body;
        while(bi<end&&depth>0){
            std::string blo=toLower(trim(lines[bi]));
            auto bw=splitWords(blo);
            if(!bw.empty()){if(isBlockStart(bw)) depth++; if(bw[0]=="done"||bw[0]=="end") depth--;}
            if(depth>0) body.push_back(lines[bi]); bi++;
        }
        FunctionDef fd; fd.params=params; fd.body=body; fd.closure=scope;
        globalFunctions[fnName]=fd;
        scope->define(fnName,Value::function(std::make_shared<FunctionDef>(fd)));
        return bi;
    }

    // ─── REPEAT ────────────────────────────────────────────
    size_t handleRepeat(const std::vector<std::string>& lines, size_t i, size_t end, ScopeRef scope,
        const std::vector<std::string>& words, const std::vector<std::string>& origWords)
    {
        int count=(int)evalExpr(origWords[1],scope)->n;
        size_t bi=i+1; int depth=1;
        std::vector<std::string> body;
        while(bi<end&&depth>0){
            std::string blo=toLower(trim(lines[bi]));
            auto bw=splitWords(blo);
            if(!bw.empty()){if(isBlockStart(bw)) depth++; if(bw[0]=="done"||bw[0]=="end") depth--;}
            if(depth>0) body.push_back(lines[bi]); bi++;
        }
        for(int c=0;c<count;c++){
            auto ls=std::make_shared<Scope>(scope);
            ls->define("iteration",Value::number(c+1));
            try{ runLines(body,ls); }
            catch(BreakSignal&){ break; }
            catch(ContinueSignal&){ continue; }
        }
        return bi;
    }

    // ─── LOOP WHILE ────────────────────────────────────────
    size_t handleLoopWhile(const std::vector<std::string>& lines, size_t i, size_t end, ScopeRef scope, const std::string& lo, const std::string& line){
        size_t dop=lo.rfind(" do");
        std::string condStr=trim(line.substr(lo.find("while")+5,dop-(lo.find("while")+5)));
        condStr=normalizeCondition(condStr);
        size_t bi=i+1; int depth=1;
        std::vector<std::string> body;
        while(bi<end&&depth>0){
            std::string blo=toLower(trim(lines[bi]));
            auto bw=splitWords(blo);
            if(!bw.empty()){if(isBlockStart(bw)) depth++; if(bw[0]=="done"||bw[0]=="end") depth--;}
            if(depth>0) body.push_back(lines[bi]); bi++;
        }
        int maxIter=10000000;
        while(evalExpr(condStr,scope)->truthy()&&maxIter-->0){
            auto ls=std::make_shared<Scope>(scope);
            try{ runLines(body,ls); }
            catch(BreakSignal&){ break; }
            catch(ContinueSignal&){ continue; }
        }
        return bi;
    }

    // ─── FOR EACH ──────────────────────────────────────────
    size_t handleForEach(const std::vector<std::string>& lines, size_t i, size_t end, ScopeRef scope,
        const std::vector<std::string>& words, const std::vector<std::string>& origWords)
    {
        std::string itemVar=origWords[2];
        std::string tblName=origWords[4];
        auto tbl=scope->get(tblName);
        size_t bi=i+1; int depth=1;
        std::vector<std::string> body;
        while(bi<end&&depth>0){
            std::string blo=toLower(trim(lines[bi]));
            auto bw=splitWords(blo);
            if(!bw.empty()){if(isBlockStart(bw)) depth++; if(bw[0]=="done"||bw[0]=="end") depth--;}
            if(depth>0) body.push_back(lines[bi]); bi++;
        }
        if(tbl->type==VType::Table){
            for(auto& v:tbl->arr){
                auto ls=std::make_shared<Scope>(scope);
                ls->define(itemVar,v);
                try{ runLines(body,ls); }
                catch(BreakSignal&){ break; }
                catch(ContinueSignal&){ continue; }
            }
            for(auto& [k,v]:tbl->tbl){
                auto ls=std::make_shared<Scope>(scope);
                ls->define(itemVar,v);
                ls->define("key",Value::string(k));
                try{ runLines(body,ls); }
                catch(BreakSignal&){ break; }
                catch(ContinueSignal&){ continue; }
            }
        } else if(tbl->type==VType::String){
            // iterate characters
            for(char c:tbl->s){
                auto ls=std::make_shared<Scope>(scope);
                ls->define(itemVar,Value::string(std::string(1,c)));
                try{ runLines(body,ls); }
                catch(BreakSignal&){ break; }
                catch(ContinueSignal&){ continue; }
            }
        }
        return bi;
    }

    // ─── FOR X FROM A TO B ─────────────────────────────────
    size_t handleForRange(const std::vector<std::string>& lines, size_t i, size_t end, ScopeRef scope,
        const std::vector<std::string>& words, const std::vector<std::string>& origWords)
    {
        std::string varName=origWords[1];
        size_t fromP=std::find(words.begin(),words.end(),"from")-words.begin();
        size_t toP  =std::find(words.begin(),words.end(),"to")  -words.begin();
        size_t stepP=std::find(words.begin(),words.end(),"step")-words.begin();
        double from2=evalExpr(origWords[fromP+1],scope)->n;
        double to2  =evalExpr(origWords[toP+1],  scope)->n;
        double step2=stepP<words.size()?evalExpr(origWords[stepP+1],scope)->n:1.0;
        if(from2<to2&&step2<=0) step2=1;
        if(from2>to2&&step2>=0) step2=-1;
        size_t bi=i+1; int depth=1;
        std::vector<std::string> body;
        while(bi<end&&depth>0){
            std::string blo=toLower(trim(lines[bi]));
            auto bw=splitWords(blo);
            if(!bw.empty()){if(isBlockStart(bw)) depth++; if(bw[0]=="done"||bw[0]=="end") depth--;}
            if(depth>0) body.push_back(lines[bi]); bi++;
        }
        for(double v2=from2; step2>0?v2<=to2:v2>=to2; v2+=step2){
            auto ls=std::make_shared<Scope>(scope);
            ls->define(varName,Value::number(v2));
            try{ runLines(body,ls); }
            catch(BreakSignal&){ break; }
            catch(ContinueSignal&){ continue; }
        }
        return bi;
    }

    // ─── TRY / CATCH ───────────────────────────────────────
    size_t handleTryCatch(const std::vector<std::string>& lines, size_t i, size_t end, ScopeRef scope){
        size_t bi=i+1; int depth=1;
        std::vector<std::string> tryBody, catchBody;
        std::string catchVar="error";
        bool inCatch=false;
        while(bi<end&&depth>0){
            std::string blo=toLower(trim(lines[bi]));
            auto bw=splitWords(blo);
            if(depth==1&&!bw.empty()&&bw[0]=="catch"){
                inCatch=true;
                // catch error as e do
                size_t asP=std::find(bw.begin(),bw.end(),"as")-bw.begin();
                if(asP<bw.size()) catchVar=bw[asP+1];
                bi++; continue;
            }
            if(!bw.empty()){if(isBlockStart(bw)) depth++; if(bw[0]=="done"||bw[0]=="end") depth--;}
            if(depth>0){
                if(!inCatch) tryBody.push_back(lines[bi]);
                else catchBody.push_back(lines[bi]);
            }
            bi++;
        }
        try {
            runLines(tryBody,std::make_shared<Scope>(scope));
        } catch(ThrowSignal& ts){
            auto cs=std::make_shared<Scope>(scope);
            cs->define(catchVar,ts.val);
            cs->define("error_message",Value::string(ts.msg));
            runLines(catchBody,cs);
        } catch(std::exception& ex){
            auto cs=std::make_shared<Scope>(scope);
            cs->define(catchVar,Value::string(ex.what()));
            cs->define("error_message",Value::string(ex.what()));
            runLines(catchBody,cs);
        }
        return bi;
    }

    // ─── MATCH ─────────────────────────────────────────────
    size_t handleMatch(const std::vector<std::string>& lines, size_t i, size_t end, ScopeRef scope,
        const std::vector<std::string>& words, const std::vector<std::string>& origWords,
        const std::string& lo, const std::string& line)
    {
        // match x with
        //   when 1 do ... done
        //   when 2 do ... done
        //   else do ... done
        // done
        std::string subject=origWords[1];
        auto val=evalExpr(subject,scope);
        size_t bi=i+1; int depth=1;
        // collect all lines of match body
        std::vector<std::string> body;
        while(bi<end&&depth>0){
            std::string blo=toLower(trim(lines[bi]));
            auto bw=splitWords(blo);
            if(!bw.empty()){
                // "when ... do" and "else do" open a block
                bool opensBlock=isBlockStart(bw)||bw[0]=="match"||
                    (bw[0]=="when"&&std::find(bw.begin(),bw.end(),"do")!=bw.end())||
                    (bw[0]=="else"&&bw.size()>1&&bw[1]=="do");
                if(opensBlock) depth++;
                if(bw[0]=="done"||bw[0]=="end") depth--;
            }
            if(depth>0) body.push_back(lines[bi]); bi++;
        }
        // process when/else arms
        bool matched=false;
        size_t j=0;
        while(j<body.size()){
            std::string blo=toLower(trim(body[j]));
            auto bw=splitWords(blo);
            if(bw.empty()){j++;continue;}
            if(bw[0]=="when"||bw[0]=="else"){
                bool match2=false;
                if(bw[0]=="else") match2=!matched;
                else {
                    // when <expr> do
                    auto origBw=splitWords(body[j]);
                    size_t doP=std::find(bw.begin(),bw.end(),"do")-bw.begin();
                    std::string caseExpr;
                    for(size_t x=1;x<doP&&x<origBw.size();x++) caseExpr+=origBw[x]+(x+1<doP?" ":"");
                    // support: when 1 or 2 or 3
                    auto parts=strSplit(caseExpr," or ");
                    for(auto& p:parts){
                        auto cv=evalExpr(trim(p),scope);
                        if(*val==*cv){match2=true;break;}
                    }
                }
                j++;
                // collect arm body
                std::vector<std::string> armBody;
                int d2=1;
                while(j<body.size()&&d2>0){
                    std::string blo2=toLower(trim(body[j]));
                    auto bw2=splitWords(blo2);
                    bool nextArm=(!bw2.empty()&&(bw2[0]=="when"||bw2[0]=="else"));
                    if(d2==1&&nextArm) break;
                    if(!bw2.empty()){if(isBlockStart(bw2)) d2++; if(bw2[0]=="done"||bw2[0]=="end") d2--;}
                    if(d2>0) armBody.push_back(body[j]);
                    j++;
                }
                if(match2&&!matched){
                    matched=true;
                    runLines(armBody,std::make_shared<Scope>(scope));
                }
            } else j++;
        }
        return bi;
    }

    // ─── GUI OPEN WINDOW ───────────────────────────────────
    size_t handleGuiOpen(const std::vector<std::string>& words, const std::vector<std::string>& origWords,
        const std::string& lo, const std::string& line, ScopeRef scope, size_t i)
    {
        // open window "title" as myWin
        std::string title="Window";
        std::string winId="main";
        size_t asP=std::find(words.begin(),words.end(),"as")-words.begin();
        std::string titleExpr=trim(line.substr(lo.find("window ")+7,
            asP<words.size()?lo.find(" as ")-(lo.find("window ")+7):std::string::npos));
        title=evalExpr(titleExpr,scope)->s;
        if(asP<words.size()) winId=origWords[asP+1];
        GuiWindow win; win.title=title;
        windows[winId]=win;
        scope->define(winId,Value::string(winId));
        return i+1;
    }
    // ─── GUI ADD WIDGET ────────────────────────────────────
    size_t handleGuiAdd(const std::vector<std::string>& words, const std::vector<std::string>& origWords,
        const std::string& lo, const std::string& line, ScopeRef scope, size_t i)
    {
        // add button "text" to myWin as myBtn onclick myFn
        // add label "text" to myWin as myLbl
        // add input "placeholder" to myWin as myInp
        std::string widgetType=words[1];
        std::string winId="main";
        std::string widgetId=widgetType+"_"+std::to_string(rand()%9000+1000);
        std::string cbFn;
        size_t toP=std::find(words.begin(),words.end(),"to")-words.begin();
        size_t asP=std::find(words.begin(),words.end(),"as")-words.begin();
        size_t onP=std::find(words.begin(),words.end(),"onclick")-words.begin();
        std::string textExpr=trim(line.substr(lo.find(widgetType+" ")+widgetType.size()+1,
            toP<words.size()?lo.find(" to ")-(lo.find(widgetType+" ")+widgetType.size()+1):std::string::npos));
        std::string text=evalExpr(textExpr,scope)->s;
        if(toP<words.size()) winId=origWords[toP+1];
        if(asP<words.size()) widgetId=origWords[asP+1];
        if(onP<words.size()) cbFn=origWords[onP+1];
        GuiWidget w; w.type=widgetType; w.id=widgetId; w.text=text; w.callbackFn=cbFn;
        auto wIt=windows.find(winId);
        if(wIt!=windows.end()) wIt->second.widgets.push_back(w);
        scope->define(widgetId,Value::string(widgetId));
        return i+1;
    }
    // ─── GUI SHOW ──────────────────────────────────────────
    size_t handleGuiShow(const std::vector<std::string>& words, ScopeRef scope, size_t i){
        // show window myWin
        std::string winId=words.size()>=3?words[2]:"main";
        auto wIt=windows.find(winId);
        if(wIt==windows.end()){ std::cerr<<"[EL1] No window: "<<winId<<"\n"; return i+1; }
        auto& win=wIt->second;
        win.shown=true;
        // Text-mode GUI (cross-platform, no deps)
        // On systems with zenity/dialog/whiptail we use them,
        // otherwise we render a beautiful TUI loop
        #ifdef _WIN32
        // Windows: use PowerShell to build a WinForms window
        std::string ps="Add-Type -AssemblyName System.Windows.Forms; ";
        ps+="$f=New-Object Windows.Forms.Form; $f.Text='"+win.title+"'; $f.Width=400; $f.Height=50; ";
        int yOff=10;
        for(auto& w:win.widgets){
            if(w.type=="label"){
                ps+="$l"+w.id+"=New-Object Windows.Forms.Label; $l"+w.id+".Text='"+w.text+"'; $l"+w.id+".Left=10; $l"+w.id+".Top="+std::to_string(yOff)+"; $l"+w.id+".Width=360; $f.Controls.Add($l"+w.id+"); ";
                yOff+=30; ps+="$f.Height+=30; ";
            } else if(w.type=="button"){
                ps+="$b"+w.id+"=New-Object Windows.Forms.Button; $b"+w.id+".Text='"+w.text+"'; $b"+w.id+".Left=10; $b"+w.id+".Top="+std::to_string(yOff)+"; $b"+w.id+".Width=360; $b"+w.id+".Add_Click({[Windows.Forms.MessageBox]::Show('"+w.text+" clicked')}); $f.Controls.Add($b"+w.id+"); ";
                yOff+=40; ps+="$f.Height+=40; ";
            } else if(w.type=="input"){
                ps+="$i"+w.id+"=New-Object Windows.Forms.TextBox; $i"+w.id+".PlaceholderText='"+w.text+"'; $i"+w.id+".Left=10; $i"+w.id+".Top="+std::to_string(yOff)+"; $i"+w.id+".Width=360; $f.Controls.Add($i"+w.id+"); ";
                yOff+=30; ps+="$f.Height+=30; ";
            }
        }
        ps+="$f.ShowDialog()";
        std::string cmd="powershell -Command \""+ps+"\"";
        std::system(cmd.c_str());
        #else
        // Unix: render a nice TUI in the terminal
        if(!noConsole){
            std::cout<<"\n╔";
            for(int ii=0;ii<42;ii++) std::cout<<"═"; std::cout<<"╗\n";
            // pad title
            int titleLen=(int)win.title.size();
            int leftPad=(42-titleLen)/2, rightPad=42-titleLen-leftPad;
            std::cout<<"║"; for(int ii=0;ii<leftPad;ii++) std::cout<<" "; std::cout<<win.title;
            for(int ii=0;ii<rightPad;ii++) std::cout<<" "; std::cout<<"║\n";
            std::cout<<"╠"; for(int ii=0;ii<42;ii++) std::cout<<"═"; std::cout<<"╣\n";
            // widgets
            for(auto& w:win.widgets){
                if(w.type=="label"||w.type=="title"||w.type=="text"){
                    std::cout<<"║  📝  "<<w.text;
                    int padLen=42-6-(int)w.text.size()-2;
                    for(int ii=0;ii<padLen;ii++) std::cout<<" ";
                    std::cout<<"  ║\n";
                } else if(w.type=="button"){
                    std::cout<<"║  "; std::cout<<"[ "<<w.text<<" ]";
                    int padLen=42-7-(int)w.text.size();
                    for(int ii=0;ii<padLen;ii++) std::cout<<" "; std::cout<<"║\n";
                } else if(w.type=="input"){
                    std::cout<<"║  ▶  "<<w.text<<": ___________________________║\n";
                }
            }
            std::cout<<"╚"; for(int ii=0;ii<42;ii++) std::cout<<"═"; std::cout<<"╝\n\n";
            // Interactive mode: let user click buttons
            bool running=true;
            while(running){
                std::cout<<"Enter button text to click (or 'close' to exit): ";
                std::string inp; std::getline(std::cin,inp);
                inp=trim(inp);
                if(inp=="close"||inp=="exit"||inp.empty()) break;
                for(auto& w:win.widgets){
                    if(w.type=="button"&&toLower(w.text)==toLower(inp)){
                        if(!w.callbackFn.empty()) callFunction(w.callbackFn,{},scope);
                        else std::cout<<"[Clicked: "<<w.text<<"]\n";
                        break;
                    }
                    if(w.type=="input"&&toLower(w.id)==toLower(inp)){
                        std::cout<<"Enter value for "<<w.text<<": ";
                        std::string val; std::getline(std::cin,val);
                        scope->set(w.id,Value::string(val));
                        std::cout<<"[Set "<<w.id<<" = "<<val<<"]\n";
                        break;
                    }
                }
            }
        }
        #endif
        return i+1;
    }
    size_t handleGuiClose(const std::vector<std::string>& words, ScopeRef scope, size_t i){
        std::string winId=words.size()>=3?words[2]:"main";
        windows.erase(winId);
        return i+1;
    }
    size_t handleGuiSetText(const std::vector<std::string>& words, const std::vector<std::string>& origWords,
        const std::string& lo, const std::string& line, ScopeRef scope, size_t i)
    {
        // set myLbl text to "new text"
        std::string widgetId=words[1];
        std::string newText=evalExpr(trim(line.substr(lo.rfind(" to ")+4)),scope)->s;
        for(auto& [wid,win]:windows){
            for(auto& w:win.widgets){
                if(w.id==widgetId){ w.text=newText; break; }
            }
        }
        return i+1;
    }

    // ─── HOWEVER IF ────────────────────────────────────────
    size_t handleHoweverIf(const std::vector<std::string>& lines, size_t i, size_t end, ScopeRef scope, const std::string& lo, const std::string& line){
        bool exitOnTrue=false, doNot=false;
        std::string rest=line.substr(line.find("if ")+3);
        if(lo.find(" then exit")!=std::string::npos){ exitOnTrue=true; rest=rest.substr(0,rest.rfind(" then exit")); }
        else if(lo.find(" then do not")!=std::string::npos){ doNot=true; rest=rest.substr(0,rest.rfind(" then do not")); }
        else if(lo.find(" then do")!=std::string::npos){ rest=rest.substr(0,rest.rfind(" then do")); }

        bool cond=evalEnglishCondition(trim(rest),scope);
        if(doNot) cond=!cond;
        if(exitOnTrue&&cond) throw ExitSignal{0};

        size_t bi=i+1; int depth=1;
        std::vector<std::string> thenBody;
        while(bi<end&&depth>0){
            std::string blo=toLower(trim(lines[bi]));
            auto bw=splitWords(blo);
            bool isEI=(!bw.empty()&&bw[0]=="also"&&bw.size()>1&&bw[1]=="if");
            bool isE =(!bw.empty()&&bw[0]=="otherwise");
            if(depth==1&&(isEI||isE)) break;
            if(!bw.empty()){if(isBlockStart(bw)) depth++; if(bw[0]=="done"||bw[0]=="end") depth--;}
            if(depth>0) thenBody.push_back(lines[bi]); bi++;
        }
        // collect else-if / else chain
        std::vector<std::pair<std::string,std::vector<std::string>>> elseIfChain;
        std::vector<std::string> elseBody;
        while(bi<end){
            std::string blo=toLower(trim(lines[bi]));
            auto bw=splitWords(blo);
            if(bw.empty()){bi++;continue;}
            if(bw[0]=="also"&&bw.size()>1&&bw[1]=="if"){
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
                    if(!bw2.empty()){if(isBlockStart(bw2)) d2++; if(bw2[0]=="done"||bw2[0]=="end") d2--;}
                    if(d2>0) eiBody.push_back(lines[bi]); bi++;
                }
                elseIfChain.push_back({trim(eiRest),eiBody});
            } else if(bw[0]=="otherwise"){
                int d2=1; bi++;
                while(bi<end&&d2>0){
                    std::string blo2=toLower(trim(lines[bi]));
                    auto bw2=splitWords(blo2);
                    if(!bw2.empty()){if(isBlockStart(bw2)) d2++; if(bw2[0]=="done"||bw2[0]=="end") d2--;}
                    if(d2>0) elseBody.push_back(lines[bi]); bi++;
                }
                break;
            } else break;
        }
        if(cond){
            runLines(thenBody,std::make_shared<Scope>(scope));
        } else {
            bool ran=false;
            for(auto& [eiCond,eiBody]:elseIfChain){
                if(evalEnglishCondition(eiCond,scope)){ runLines(eiBody,std::make_shared<Scope>(scope)); ran=true; break; }
            }
            if(!ran&&!elseBody.empty()) runLines(elseBody,std::make_shared<Scope>(scope));
        }
        return bi;
    }

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
        if(w[0]=="for") return true;
        if(w[0]=="make"&&w.size()>1&&w[1]=="function") return true;
        if(w[0]=="function") return true;
        if(w[0]=="try") return true;
        if(w[0]=="on"&&w.size()>1&&w[1]=="event") return true;
        if(w[0]=="match") return true;
        return false;
    }

    // ─── ENGLISH CONDITION ─────────────────────────────────
    bool evalEnglishCondition(const std::string& raw, ScopeRef scope){
        std::string lo=toLower(trim(raw));
        auto words=splitWords(lo);
        if(words.empty()) return false;
        // find "is"
        size_t isPos=std::find(words.begin(),words.end(),"is")-words.begin();
        if(isPos>=words.size()) return evalExpr(normalizeCondition(raw),scope)->truthy();

        auto origWords2=splitWords(raw);
        std::string varPart;
        for(size_t x=0;x<isPos;x++) varPart+=origWords2[x]+(x+1<isPos?" ":"");
        ValueRef lhs=evalExpr(varPart,scope);

        std::vector<std::string> rhs(words.begin()+isPos+1,words.end());
        std::vector<std::string> rhsOrig(origWords2.begin()+isPos+1,origWords2.end());
        if(rhs.empty()) return lhs->truthy();

        bool negated=false; size_t ri=0;
        if(ri<rhs.size()&&rhs[ri]=="not"){ negated=true; ri++; }

        if(ri<rhs.size()&&rhs[ri]=="empty"){
            bool empty=(lhs->type==VType::Nil)||(lhs->type==VType::String&&lhs->s.empty())||(lhs->type==VType::Table&&lhs->arr.empty()&&lhs->tbl.empty());
            return negated?!empty:empty;
        }
        if(ri<rhs.size()&&(rhs[ri]=="true"||rhs[ri]=="false"||rhs[ri]=="nil")){
            if(rhs[ri]=="nil") { bool res=lhs->type==VType::Nil; return negated?!res:res; }
            bool res=rhs[ri]=="true"?lhs->truthy():!lhs->truthy();
            return negated?!res:res;
        }
        // comparison
        if(ri<rhs.size()&&(rhs[ri]=="greater"||rhs[ri]=="less"||rhs[ri]=="more")){
            bool greater=(rhs[ri]=="greater"||rhs[ri]=="more"); ri++;
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
            bool result=greater?(orEq?lhs->n>=rval->n:lhs->n>rval->n):(orEq?lhs->n<=rval->n:lhs->n<rval->n);
            return negated?!result:result;
        }
        // contains
        if(ri<rhs.size()&&rhs[ri]=="contains"){
            ri++;
            std::string needle;
            for(size_t x=ri;x<rhsOrig.size();x++) needle+=rhsOrig[x]+(x+1<rhsOrig.size()?" ":"");
            auto nv=evalExpr(trim(needle),scope);
            bool res=false;
            if(lhs->type==VType::String) res=lhs->s.find(nv->s)!=std::string::npos;
            else if(lhs->type==VType::Table){ for(auto& v:lhs->arr) if(*v==*nv){res=true;break;} }
            return negated?!res:res;
        }
        // starts with / ends with
        if(ri<rhs.size()&&rhs[ri]=="starts"&&ri+2<rhs.size()&&rhs[ri+1]=="with"){
            ri+=2;
            std::string needle; for(size_t x=ri;x<rhsOrig.size();x++) needle+=rhsOrig[x]+(x+1<rhsOrig.size()?" ":"");
            auto nv=evalExpr(trim(needle),scope);
            bool res=lhs->s.rfind(nv->s,0)==0;
            return negated?!res:res;
        }
        if(ri<rhs.size()&&rhs[ri]=="ends"&&ri+2<rhs.size()&&rhs[ri+1]=="with"){
            ri+=2;
            std::string needle; for(size_t x=ri;x<rhsOrig.size();x++) needle+=rhsOrig[x]+(x+1<rhsOrig.size()?" ":"");
            auto nv=evalExpr(trim(needle),scope);
            bool res=lhs->s.size()>=nv->s.size()&&lhs->s.substr(lhs->s.size()-nv->s.size())==nv->s;
            return negated?!res:res;
        }
        // either / or matching
        if(ri<rhs.size()&&rhs[ri]=="either") ri++;
        std::vector<std::string> candidates; std::string cur;
        for(size_t x=ri;x<rhs.size();x++){
            if(rhs[x]=="or"||rhs[x]=="and"){ if(!cur.empty()) candidates.push_back(trim(cur)); cur=""; }
            else cur+=rhsOrig[x]+" ";
        }
        if(!cur.empty()) candidates.push_back(trim(cur));
        bool matched=false;
        for(auto& c:candidates){ auto rv=evalExpr(c,scope); if(*lhs==*rv){matched=true;break;} }
        return negated?!matched:matched;
    }

    std::string normalizeCondition(const std::string& s){
        std::string r=s;
        auto rpl=[&](const std::string& f,const std::string& t){ size_t p=0; while((p=r.find(f,p))!=std::string::npos){r.replace(p,f.size(),t);p+=t.size();} };
        rpl(" is equal to "," == "); rpl(" is not equal to "," != ");
        rpl(" is greater than or equal to "," >= "); rpl(" is less than or equal to "," <= ");
        rpl(" is greater than "," > "); rpl(" is less than "," < ");
        rpl(" is not "," != "); rpl(" is "," == ");
        return r;
    }

    // ─── CALL FUNCTION ─────────────────────────────────────
    ValueRef callFunction(const std::string& name, const std::vector<ValueRef>& args, ScopeRef scope){
        // Built-ins
        if(name=="print"||name=="say"){ if(!noConsole){ for(auto& a:args) std::cout<<a->repr(); std::cout<<"\n"; } return Value::nil(); }
        if(name=="write"){ if(!noConsole){ for(auto& a:args) std::cout<<a->repr(); std::cout<<std::flush; } return Value::nil(); }
        if(name=="input"){ if(!noConsole){ if(!args.empty()) std::cout<<args[0]->repr(); std::string inp; std::getline(std::cin,inp); return isNumber(inp)?Value::number(std::stod(inp)):Value::string(inp); } return Value::string(""); }
        if(name=="tostring"){ return args.empty()?Value::string(""):Value::string(args[0]->repr()); }
        if(name=="tonumber"){ if(args.empty()) return Value::number(0); if(isNumber(args[0]->repr())) return Value::number(std::stod(args[0]->repr())); return Value::nil(); }
        if(name=="typeof"){ if(args.empty()) return Value::string("nil"); switch(args[0]->type){ case VType::Nil: return Value::string("nil"); case VType::Bool: return Value::string("bool"); case VType::Number: return Value::string("number"); case VType::String: return Value::string("string"); case VType::Table: return Value::string("table"); case VType::Function: return Value::string("function"); } }
        if(name=="len"||name=="length"){ if(args.empty()) return Value::number(0); if(args[0]->type==VType::String) return Value::number(args[0]->s.size()); if(args[0]->type==VType::Table) return Value::number(args[0]->arr.size()+args[0]->tbl.size()); return Value::number(0); }
        if(name=="upper"){ return args.empty()?Value::nil():Value::string(toUpper(args[0]->s)); }
        if(name=="lower"){ return args.empty()?Value::nil():Value::string(toLower(args[0]->s)); }
        if(name=="trim2"||name=="trimstr"){ return args.empty()?Value::nil():Value::string(trim(args[0]->s)); }
        if(name=="split"){ if(args.size()<2) return Value::table(); auto r=Value::table(); for(auto& p:strSplit(args[0]->s,args[1]->s)) r->arr.push_back(Value::string(p)); return r; }
        if(name=="join"){ if(args.size()<2) return Value::string(""); std::string out; for(size_t ii=0;ii<args[0]->arr.size();ii++){if(ii)out+=args[1]->s;out+=args[0]->arr[ii]->repr();} return Value::string(out); }
        if(name=="replace"){ if(args.size()<3) return Value::nil(); return Value::string(strReplace(args[0]->s,args[1]->s,args[2]->s)); }
        if(name=="contains"){ if(args.size()<2) return Value::boolean(false); return Value::boolean(args[0]->s.find(args[1]->s)!=std::string::npos); }
        if(name=="startswith"){ if(args.size()<2) return Value::boolean(false); return Value::boolean(args[0]->s.rfind(args[1]->s,0)==0); }
        if(name=="endswith"){ if(args.size()<2) return Value::boolean(false); return Value::boolean(args[0]->s.size()>=args[1]->s.size()&&args[0]->s.substr(args[0]->s.size()-args[1]->s.size())==args[1]->s); }
        if(name=="substr"){ if(args.size()<2) return Value::nil(); size_t start=(size_t)args[1]->n; size_t len2=args.size()>=3?(size_t)args[2]->n:std::string::npos; return Value::string(args[0]->s.substr(start,len2)); }
        if(name=="charat"){ if(args.size()<2) return Value::nil(); size_t idx=(size_t)args[1]->n; return idx<args[0]->s.size()?Value::string(std::string(1,args[0]->s[idx])):Value::nil(); }
        if(name=="charcode"){ if(args.empty()) return Value::nil(); return args[0]->s.empty()?Value::nil():Value::number((int)args[0]->s[0]); }
        if(name=="fromcharcode"){ return args.empty()?Value::nil():Value::string(std::string(1,(char)(int)args[0]->n)); }
        if(name=="sqrt")  return args.empty()?Value::number(0):Value::number(std::sqrt(args[0]->n));
        if(name=="abs")   return args.empty()?Value::number(0):Value::number(std::abs(args[0]->n));
        if(name=="floor") return args.empty()?Value::number(0):Value::number(std::floor(args[0]->n));
        if(name=="ceil")  return args.empty()?Value::number(0):Value::number(std::ceil(args[0]->n));
        if(name=="round") return args.empty()?Value::number(0):Value::number(std::round(args[0]->n));
        if(name=="pow")   return args.size()<2?Value::number(0):Value::number(std::pow(args[0]->n,args[1]->n));
        if(name=="min")   return args.size()<2?Value::number(0):Value::number(std::min(args[0]->n,args[1]->n));
        if(name=="max")   return args.size()<2?Value::number(0):Value::number(std::max(args[0]->n,args[1]->n));
        if(name=="clamp") return args.size()<3?Value::number(0):Value::number(std::max(args[1]->n,std::min(args[2]->n,args[0]->n)));
        if(name=="random"){ double lo2=0,hi2=1; if(args.size()>=2){lo2=args[0]->n;hi2=args[1]->n;}else if(args.size()==1){hi2=args[0]->n;} return Value::number(lo2+(hi2-lo2)*((double)rand()/(double)RAND_MAX)); }
        if(name=="randomint"){ int lo2=0,hi2=100; if(args.size()>=2){lo2=(int)args[0]->n;hi2=(int)args[1]->n;}else if(args.size()==1){hi2=(int)args[0]->n;} return Value::number(lo2+rand()%(hi2-lo2+1)); }
        if(name=="time"||name=="now"){ auto t=std::chrono::system_clock::now().time_since_epoch(); return Value::number((double)std::chrono::duration_cast<std::chrono::milliseconds>(t).count()); }
        if(name=="timestamp"){ std::time_t t=std::time(nullptr); char buf[64]; std::strftime(buf,sizeof(buf),"%Y-%m-%d %H:%M:%S",std::localtime(&t)); return Value::string(buf); }
        if(name=="encode_json"||name=="tojson"){ return args.empty()?Value::string("null"):Value::string(encodeJson(args[0])); }
        if(name=="decode_json"||name=="fromjson"){ if(args.empty()) return Value::nil(); size_t p=0; return decodeJson(args[0]->s,p); }
        if(name=="keys"){ if(args.empty()) return Value::table(); auto r=Value::table(); for(auto& [k,_]:args[0]->tbl) r->arr.push_back(Value::string(k)); return r; }
        if(name=="values"){ if(args.empty()) return Value::table(); auto r=Value::table(); for(auto& [_,v]:args[0]->tbl) r->arr.push_back(v); return r; }
        if(name=="table_push"||name=="push"){ if(args.size()>=2) args[0]->arr.push_back(args[1]); return args.empty()?Value::nil():args[0]; }
        if(name=="table_pop"||name=="pop"){ if(!args.empty()&&!args[0]->arr.empty()){auto v=args[0]->arr.back();args[0]->arr.pop_back();return v;} return Value::nil(); }
        if(name=="table_insert"){ if(args.size()>=3){ size_t idx=(size_t)args[1]->n; if(idx<=args[0]->arr.size()) args[0]->arr.insert(args[0]->arr.begin()+idx,args[2]); } return Value::nil(); }
        if(name=="table_slice"){ if(args.size()<3) return Value::table(); auto r=Value::table(); size_t s2=(size_t)args[1]->n,e2=(size_t)args[2]->n; for(size_t ii=s2;ii<e2&&ii<args[0]->arr.size();ii++) r->arr.push_back(args[0]->arr[ii]); return r; }
        if(name=="env"){ if(args.empty()) return Value::nil(); const char* ev=std::getenv(args[0]->s.c_str()); return ev?Value::string(ev):Value::nil(); }
        if(name=="exec"){ if(args.empty()) return Value::nil(); std::string out2; FILE* pp=popen(args[0]->s.c_str(),"r"); if(pp){char buf[4096];while(fgets(buf,sizeof(buf),pp))out2+=buf;pclose(pp);} return Value::string(trim(out2)); }
        if(name=="exit2"){ throw ExitSignal{args.empty()?0:(int)args[0]->n}; }
        // user-defined
        auto fv=scope->get(name);
        if(fv->type==VType::Function&&fv->fn){
            auto fnScope=std::make_shared<Scope>(fv->fn->closure);
            for(size_t p=0;p<fv->fn->params.size();p++) fnScope->define(fv->fn->params[p],p<args.size()?args[p]:Value::nil());
            try{ runLines(fv->fn->body,fnScope); } catch(ReturnSignal& r){ return r.val; }
            return Value::nil();
        }
        auto git=globalFunctions.find(name);
        if(git!=globalFunctions.end()){
            auto& fd=git->second;
            auto fnScope=std::make_shared<Scope>(fd.closure?fd.closure:globalScope);
            for(size_t p=0;p<fd.params.size();p++) fnScope->define(fd.params[p],p<args.size()?args[p]:Value::nil());
            try{ runLines(fd.body,fnScope); } catch(ReturnSignal& r){ return r.val; }
            return Value::nil();
        }
        return Value::nil();
    }

    // ─── EVAL EXPRESSION ───────────────────────────────────
    ValueRef evalExpr(const std::string& expr, ScopeRef scope){
        std::string e=trim(expr);
        if(e.empty()) return Value::nil();
        if(e=="nil"||e=="nothing"||e=="null") return Value::nil();
        if(e=="true") return Value::boolean(true);
        if(e=="false") return Value::boolean(false);
        if(isNumber(e)) return Value::number(std::stod(e));
        std::string lo2=toLower(e);
        auto dit=defines.find(lo2);
        if(dit!=defines.end()) return dit->second;
        // single-token string
        if(e.size()>=2&&((e.front()=='"'&&e.back()=='"')||(e.front()=='\''&&e.back()=='\''))){
            bool single=true; char q=e.front();
            for(size_t qi=1;qi<e.size()-1;qi++){ if(e[qi]=='\\'){qi++;continue;} if(e[qi]==q){single=false;break;} }
            if(single){
                std::string out;
                for(size_t qi=1;qi<e.size()-1;qi++){
                    if(e[qi]=='\\'){ qi++; if(qi<e.size()-1){ switch(e[qi]){case 'n':out+='\n';break;case 't':out+='\t';break;case 'r':out+='\r';break;default:out+=e[qi];} } }
                    else out+=e[qi];
                }
                return Value::string(out);
            }
        }
        // function call with parens
        {
            size_t lp=e.find('(');
            if(lp!=std::string::npos&&e.back()==')'){
                std::string fnName=trim(e.substr(0,lp));
                bool validId=!fnName.empty();
                for(char c:fnName) if(!std::isalnum((unsigned char)c)&&c!='_') validId=false;
                if(validId){
                    std::string argStr=e.substr(lp+1,e.size()-lp-2);
                    auto args=parseArgList(argStr,scope);
                    return callFunction(fnName,args,scope);
                }
            }
        }
        ExprParser ep(e,scope,defines);
        return ep.parse();
    }

    std::vector<ValueRef> parseArgList(const std::string& s, ScopeRef scope){
        std::vector<ValueRef> result;
        // normalise " and " -> "," outside of string literals
        std::string src;
        {
            bool inS=false; char sq2=0;
            for(size_t ii=0;ii<s.size();ii++){
                if(inS){src+=s[ii];if(s[ii]==sq2)inS=false;continue;}
                if(s[ii]=='"'||s[ii]=='\''){inS=true;sq2=s[ii];src+=s[ii];continue;}
                if(ii+5<=s.size()&&s.substr(ii,5)==" and "){src+=",";ii+=4;continue;}
                src+=s[ii];
            }
        }
        int depth=0; bool inStr=false; char sq=0; std::string cur;
        for(size_t i=0;i<src.size();i++){
            char c=src[i];
            if(inStr){cur+=c;if(c==sq)inStr=false;continue;}
            if(c=='"'||c=='\''){inStr=true;sq=c;cur+=c;continue;}
            if(c=='('||c=='[') depth++;
            if(c==')'||c==']') depth--;
            if(depth==0&&c==','){
                std::string t=trim(cur);
                if(!t.empty()) result.push_back(evalExpr(t,scope));
                cur="";
            } else cur+=c;
        }
        std::string t=trim(cur);
        if(!t.empty()) result.push_back(evalExpr(t,scope));
        return result;
    }

    void runFile(const std::string& filename, ScopeRef scope=nullptr){
        if(!scope) scope=globalScope;
        std::ifstream f(filename);
        if(!f){ std::cerr<<"[EL1] Cannot open: "<<filename<<"\n"; return; }
        // set scriptDir
        std::string dir=filename;
        size_t sl=dir.rfind('/'); if(sl==std::string::npos) sl=dir.rfind('\\');
        if(sl!=std::string::npos) scriptDir=dir.substr(0,sl);
        std::vector<std::string> lines; std::string line2;
        while(std::getline(f,line2)) lines.push_back(line2);
        // set args
        auto argsTable=Value::table();
        argsTable->arr.push_back(Value::string(filename));
        scope->define("args",argsTable);
        try{ runLines(lines,scope); }
        catch(ExitSignal& e2){ std::exit(e2.code); }
        catch(ThrowSignal& ts){ std::cerr<<"[EL1 Uncaught Error] "<<ts.msg<<"\n"; std::exit(1); }
        catch(std::exception& ex){ std::cerr<<"[EL1 Error] "<<ex.what()<<"\n"; std::exit(1); }
    }
};

// ─────────────────────────────────────────────────────────────
//  main
// ─────────────────────────────────────────────────────────────
int main(int argc, char* argv[]){
    if(argc<2){
        std::cout<<"╔══════════════════════════════════════════╗\n";
        std::cout<<"║    EasyLang (EL1) v2.0 Interpreter       ║\n";
        std::cout<<"╠══════════════════════════════════════════╣\n";
        std::cout<<"║  Usage: el <script.el> [options]         ║\n";
        std::cout<<"║  Options:                                ║\n";
        std::cout<<"║    -noconsole   Suppress all output      ║\n";
        std::cout<<"║    -faster      Skip startup checks      ║\n";
        std::cout<<"║    -optimize    Enable optimizer hints   ║\n";
        std::cout<<"║    -version     Print version info       ║\n";
        std::cout<<"╚══════════════════════════════════════════╝\n";
        return 0;
    }
    std::string scriptFile;
    bool noConsole=false,faster=false,optimize=false;
    for(int a=1;a<argc;a++){
        std::string arg=argv[a];
        if(arg=="-noconsole") noConsole=true;
        else if(arg=="-faster") faster=true;
        else if(arg=="-optimize") optimize=true;
        else if(arg=="-version"){ std::cout<<"EasyLang EL1 v2.0.0\n"; return 0; }
        else if(arg[0]!='-'){ scriptFile=arg; }
    }
    if(scriptFile.empty()){ std::cerr<<"[EL1] No script file provided.\n"; return 1; }
    struct stat st;
    if(stat(scriptFile.c_str(),&st)!=0){ std::cerr<<"[EL1] File not found: "<<scriptFile<<"\n"; return 1; }
    // pass extra args into args table
    Interpreter interp;
    interp.noConsole=noConsole;
    interp.faster=faster;
    interp.optimize=optimize;
    srand((unsigned)std::time(nullptr));
    interp.runFile(scriptFile);
    return 0;
}
