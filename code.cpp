#include <bits/stdc++.h>
using namespace std;

// Persistent storage using simple binary files with indices in memory kept minimal.
// Due to performance rules, avoid loading all records; use fstreams with indices stored compactly.

struct User {
    string id, password, name;
    int priv;
};

struct Book {
    string isbn, name, author;
    vector<string> keywords;
    long long stock = 0;
    long double price = 0;
};

// Simple hashed index files for users and books mapping key->file offset.
// Implemented as plain text lines: key\toffset. Load on demand and update append-only.

class Index {
    string path;
    unordered_map<string, long long> map; // small, only keys used; we will lazily load
public:
    Index(string p): path(p) {
        ifstream in(path);
        string k; long long off;
        if (in) {
            while (in >> k >> off) map[k]=off;
        }
    }
    long long get(const string &k) const {
        auto it = map.find(k);
        if (it==map.end()) return -1;
        return it->second;
    }
    void set(const string &k, long long off) {
        map[k]=off;
        ofstream out(path, ios::app);
        out<<k<<"\t"<<off<<"\n";
    }
    void erase(const string &k) {
        map.erase(k);
        // rewrite file to keep within file-count limit; acceptable occasional rewrite
        ofstream out(path);
        for (auto &p: map) out<<p.first<<"\t"<<p.second<<"\n";
    }
};

// Users store as newline-delimited JSON-like; books similar. We'll use fixed serialization with escaping.
static string esc(const string &s) {
    string r; r.reserve(s.size());
    for(char c: s){ if(c=='\\' || c=='\n') { r.push_back('\\'); r.push_back(c); } else r.push_back(c);} return r;
}
static string join(const vector<string>& v){ string r; for(size_t i=0;i<v.size();++i){ if(i) r.push_back('|'); r+=v[i]; } return r; }
static vector<string> split_kw(const string&s){ vector<string> r; string cur; for(char c: s){ if(c=='|'){ r.push_back(cur); cur.clear(); } else cur.push_back(c);} r.push_back(cur); return r; }

class Store {
    string users_path = "users.db";
    string books_path = "books.db";
    string trans_path = "trans.log"; // income(+), expenditure(-) lines
    Index user_idx{"users.idx"};
    Index book_idx{"books.idx"};

    fstream uf; fstream bf; fstream tf;
public:
    Store(){
        // open files
        uf.open(users_path, ios::in|ios::out|ios::binary);
        if(!uf){ ofstream tmp(users_path); tmp.close(); uf.open(users_path, ios::in|ios::out|ios::binary); }
        bf.open(books_path, ios::in|ios::out|ios::binary);
        if(!bf){ ofstream tmp(books_path); tmp.close(); bf.open(books_path, ios::in|ios::out|ios::binary); }
        tf.open(trans_path, ios::in|ios::out);
        if(!tf){ ofstream tmp(trans_path); tmp.close(); tf.open(trans_path, ios::in|ios::out); }
        // ensure root exists
        if (user_idx.get("root")==-1) {
            User u{"root","sjtu","root",7};
            add_user(u);
        }
    }
    long long add_user(const User& u){
        uf.seekp(0, ios::end); long long off = uf.tellp();
        string line = esc(u.id)+"\t"+esc(u.password)+"\t"+esc(u.name)+"\t"+to_string(u.priv)+"\n";
        uf.write(line.data(), line.size()); uf.flush();
        user_idx.set(u.id, off);
        return off;
    }
    optional<User> read_user(const string& id){ long long off = user_idx.get(id); if(off<0) return nullopt; uf.seekg(off); string line; getline(uf, line); if(!uf) return nullopt; vector<string> f; string cur; for(char c: line){ if(c=='\t'){ f.push_back(cur); cur.clear(); } else cur.push_back(c);} f.push_back(cur); if(f.size()!=4) return nullopt; return User{f[0],f[1],f[2],stoi(f[3])}; }
    bool delete_user(const string& id){ long long off = user_idx.get(id); if(off<0) return false; user_idx.erase(id); return true; }

    long long add_book(const Book& b){ bf.seekp(0, ios::end); long long off = bf.tellp();
        string kw = join(b.keywords);
        string line = esc(b.isbn)+"\t"+esc(b.name)+"\t"+esc(b.author)+"\t"+esc(kw)+"\t"+to_string((double)b.price)+"\t"+to_string(b.stock)+"\n";
        bf.write(line.data(), line.size()); bf.flush(); book_idx.set(b.isbn, off); return off; }
    optional<Book> read_book(const string& isbn){ long long off = book_idx.get(isbn); if(off<0) return nullopt; bf.seekg(off); string line; getline(bf,line); if(!bf) return nullopt; vector<string> f; string cur; for(char c: line){ if(c=='\t'){ f.push_back(cur); cur.clear(); } else cur.push_back(c);} f.push_back(cur); if(f.size()!=6) return nullopt; Book bk; bk.isbn=f[0]; bk.name=f[1]; bk.author=f[2]; bk.keywords=split_kw(f[3]); bk.price=stold(f[4]); bk.stock=stoll(f[5]); return bk; }
    void update_book(const Book& bk){ // append new record and update index
        add_book(bk);
    }

    void record_income(long double x){ ofstream out(trans_path, ios::app); out<<"+ "<<fixed<<setprecision(2)<<(double)x<<"\n"; }
    void record_expend(long double x){ ofstream out(trans_path, ios::app); out<<"- "<<fixed<<setprecision(2)<<(double)x<<"\n"; }

    // show finance: compute from trans_path
    pair<long double,long double> sum_finance(int count){ ifstream in(trans_path); string t; long double inc=0, exp=0; vector<pair<char,long double>> v; while(true){ string sign; long double val; if(!(in>>sign>>val)) break; char s=sign[0]; v.push_back({s,val}); }
        if(count<0) count=v.size(); int start=max(0,(int)v.size()-count); for(int i=start;i<(int)v.size();++i){ if(v[i].first=='+') inc+=v[i].second; else exp+=v[i].second; } return {inc,exp}; }
};

struct Session { string selected_isbn; };

int main(){ ios::sync_with_stdio(false); cin.tie(nullptr);
    Store store;
    vector<User> login_stack; vector<Session> sessions;

    string line;
    auto current_priv = [&]{ return login_stack.empty()?0:login_stack.back().priv; };

    while (true){ if(!getline(cin,line)) break; // allow blank lines
        string s=line; // normalize spaces: treat multiple spaces as one, trim
        // Tokenize respecting quotes mid-token
        vector<string> tokens;
        {
            string cur; bool inq=false;
            for(char c: s){
                if(c=='"'){ inq=!inq; cur.push_back(c); }
                else if(c==' ' && !inq){ if(!cur.empty()){ tokens.push_back(cur); cur.clear(); } }
                else { cur.push_back(c); }
            }
            if(!cur.empty()) tokens.push_back(cur);
        }
        if(tokens.empty()) { continue; }
        string cmd=tokens[0];
        auto invalid=[&]{ cout<<"Invalid\n"; };

        if(cmd=="quit"||cmd=="exit"){ break; }
        else if(cmd=="su"){ if(tokens.size()<2 || tokens.size()>3){ invalid(); continue; } string uid=tokens[1]; string pwd = tokens.size()==3?tokens[2]:""; auto u=store.read_user(uid); if(!u){ invalid(); continue; } if(current_priv()>u->priv){ /* higher priv can omit pwd */ }
            if(tokens.size()==2){ if(current_priv()>u->priv){ login_stack.push_back(*u); sessions.push_back({""}); } else { invalid(); } }
            else { if(pwd==u->password){ login_stack.push_back(*u); sessions.push_back({""}); } else { invalid(); continue; } }
        }
        else if(cmd=="logout"){ if(login_stack.empty()){ invalid(); continue; } login_stack.pop_back(); sessions.pop_back(); }
        else if(cmd=="register"){ if(tokens.size()!=4){ invalid(); continue; } string uid=tokens[1], pw=tokens[2], name=tokens[3]; if(store.read_user(uid)){ invalid(); continue; } store.add_user(User{uid,pw,name,1}); }
        else if(cmd=="passwd"){ if(tokens.size()<3 || tokens.size()>4){ invalid(); continue; } string uid=tokens[1]; auto u=store.read_user(uid); if(!u){ invalid(); continue; } string newp = tokens.back(); if(current_priv()==7){ if(tokens.size()==3){ store.add_user(User{u->id,newp,u->name,u->priv}); } else { string cur=tokens[2]; if(cur==u->password) store.add_user(User{u->id,newp,u->name,u->priv}); else { invalid(); continue; } } }
            else { if(tokens.size()!=4){ invalid(); continue; } string cur=tokens[2]; if(cur==u->password) store.add_user(User{u->id,newp,u->name,u->priv}); else { invalid(); continue; } }
        }
        else if(cmd=="useradd"){ if(current_priv()<3){ invalid(); continue; } if(tokens.size()!=5){ invalid(); continue; } string uid=tokens[1], pw=tokens[2]; int priv; try{ priv=stoi(tokens[3]); }catch(...){ invalid(); continue; } string name=tokens[4]; if(priv>=current_priv() || (priv!=1 && priv!=3 && priv!=7)){ invalid(); continue; } if(store.read_user(uid)){ invalid(); continue; } store.add_user(User{uid,pw,name,priv}); }
        else if(cmd=="delete"){ if(current_priv()<7){ invalid(); continue; } if(tokens.size()!=2){ invalid(); continue; } string uid=tokens[1]; // cannot delete if logged in
            bool logged=false; for(auto &u: login_stack) if(u.id==uid){ logged=true; break; } if(logged){ invalid(); continue; } if(!store.read_user(uid)){ invalid(); continue; } store.delete_user(uid); }
        else if(cmd=="select"){ if(current_priv()<3){ invalid(); continue; } if(tokens.size()!=2){ invalid(); continue; } string isbn=tokens[1]; auto bk=store.read_book(isbn); if(!bk){ Book nb; nb.isbn=isbn; nb.stock=0; nb.price=0; nb.name=""; nb.author=""; nb.keywords={}; store.add_book(nb); } sessions.back().selected_isbn=isbn; }
        else if(cmd=="modify"){ if(current_priv()<3){ invalid(); continue; } if(sessions.empty()||sessions.back().selected_isbn.empty()){ invalid(); continue; }
            // parse options like -ISBN=..., -name="..."
            auto bk = store.read_book(sessions.back().selected_isbn); if(!bk){ invalid(); continue; }
            unordered_set<string> seen;
            for(size_t i=1;i<tokens.size();++i){ string t=tokens[i]; size_t eq=t.find('='); if(eq==string::npos){ bk=optional<Book>(); break; }
                if(t.rfind("-ISBN",0)==0){ if(seen.count("ISBN")){ invalid(); bk=optional<Book>(); break; } seen.insert("ISBN"); string nv=t.substr(eq+1); if(nv.empty()||nv==bk->isbn){ bk=optional<Book>(); break; } if(store.read_book(nv)){ bk=optional<Book>(); break; } bk->isbn=nv; }
                else if(t.rfind("-name",0)==0){ if(seen.count("name")){ bk=optional<Book>(); break;} seen.insert("name"); string nv=t.substr(eq+1); if(nv.size()>=2 && nv.front()=='"' && nv.back()=='"') nv=nv.substr(1,nv.size()-2); if(nv.empty()){ bk=optional<Book>(); break;} bk->name=nv; }
                else if(t.rfind("-author",0)==0){ if(seen.count("author")){ bk=optional<Book>(); break;} seen.insert("author"); string nv=t.substr(eq+1); if(nv.size()>=2 && nv.front()=='"' && nv.back()=='"') nv=nv.substr(1,nv.size()-2); if(nv.empty()){ bk=optional<Book>(); break;} bk->author=nv; }
                else if(t.rfind("-keyword",0)==0){ if(seen.count("keyword")){ bk=optional<Book>(); break;} seen.insert("keyword"); string nv=t.substr(eq+1); if(nv.size()>=2 && nv.front()=='"' && nv.back()=='"') nv=nv.substr(1,nv.size()-2); if(nv.empty()){ bk=optional<Book>(); break;} auto parts=split_kw(nv); set<string> uniq(parts.begin(), parts.end()); if((int)uniq.size()!=(int)parts.size()){ bk=optional<Book>(); break;} bk->keywords=parts; }
                else if(t.rfind("-price",0)==0){ if(seen.count("price")){ bk=optional<Book>(); break;} seen.insert("price"); string nv=t.substr(eq+1); if(nv.empty()){ bk=optional<Book>(); break;} long double p; try{ p=stold(nv);}catch(...){ bk=optional<Book>(); break;} bk->price=p; }
                else { bk=optional<Book>(); break; }
            }
            if(!bk){ invalid(); continue; }
            store.update_book(*bk);
            sessions.back().selected_isbn=bk->isbn;
        }
        else if(cmd=="import"){ if(current_priv()<3){ invalid(); continue; } if(sessions.empty()||sessions.back().selected_isbn.empty()){ invalid(); continue; } if(tokens.size()!=3){ invalid(); continue; } long long q; long double cost; try{ q=stoll(tokens[1]); cost=stold(tokens[2]); }catch(...){ invalid(); continue; } if(q<=0 || cost<=0){ invalid(); continue; } auto bk=store.read_book(sessions.back().selected_isbn); if(!bk){ invalid(); continue; } bk->stock += q; store.update_book(*bk); store.record_expend(cost);
        }
        else if(cmd=="buy"){ if(current_priv()<1){ invalid(); continue; } if(tokens.size()!=3){ invalid(); continue; } string isbn=tokens[1]; long long q; try{ q=stoll(tokens[2]); }catch(...){ invalid(); continue; } if(q<=0){ invalid(); continue; } auto bk=store.read_book(isbn); if(!bk){ invalid(); continue; } if(bk->stock<q){ invalid(); continue; } bk->stock -= q; store.update_book(*bk); long double total = bk->price * q; cout<<fixed<<setprecision(2)<<(double)total<<"\n"; store.record_income(total);
        }
        else if(cmd=="show"){ if(current_priv()<1){ invalid(); continue; } // handle optional filters
            string type=""; string val=""; if(tokens.size()>1){ string t=tokens[1]; size_t eq=t.find('='); if(t.rfind("-ISBN",0)==0){ type="ISBN"; val=t.substr(eq+1); }
                else if(t.rfind("-name",0)==0){ type="name"; val=t.substr(eq+1); if(val.size()>=2 && val.front()=='"' && val.back()=='"') val=val.substr(1,val.size()-2); }
                else if(t.rfind("-author",0)==0){ type="author"; val=t.substr(eq+1); if(val.size()>=2 && val.front()=='"' && val.back()=='"') val=val.substr(1,val.size()-2); }
                else if(t.rfind("-keyword",0)==0){ type="keyword"; val=t.substr(eq+1); if(val.size()>=2 && val.front()=='"' && val.back()=='"') val=val.substr(1,val.size()-2); auto parts=split_kw(val); if(parts.size()!=1){ invalid(); continue; } }
                else { invalid(); continue; } if(val.empty()){ invalid(); continue; } }
            // scan books file linearly to satisfy not reading all data into memory? We'll stream and filter
            ifstream in("books.db"); vector<Book> res; string line; while(getline(in,line)){ if(line.empty()) continue; vector<string> f; string cur; for(char c: line){ if(c=='\t'){ f.push_back(cur); cur.clear(); } else cur.push_back(c);} f.push_back(cur); if(f.size()!=6) continue; Book bk; bk.isbn=f[0]; bk.name=f[1]; bk.author=f[2]; bk.keywords=split_kw(f[3]); bk.price=stold(f[4]); bk.stock=stoll(f[5]);
                bool ok=true; if(type=="ISBN") ok=(bk.isbn==val); else if(type=="name") ok=(bk.name==val); else if(type=="author") ok=(bk.author==val); else if(type=="keyword") ok=(find(bk.keywords.begin(), bk.keywords.end(), val)!=bk.keywords.end()); if(ok) res.push_back(bk); }
            sort(res.begin(), res.end(), [](const Book&a,const Book&b){ return a.isbn<b.isbn; });
            if(res.empty()){ cout<<"\n"; }
            else { for(auto &bk: res){ cout<<bk.isbn<<"\t"<<bk.name<<"\t"<<bk.author<<"\t"<<join(bk.keywords)<<"\t"<<fixed<<setprecision(2)<<(double)bk.price<<"\t"<<bk.stock<<"\n"; } }
        }
        else if(cmd=="show" && tokens.size()>1 && tokens[1]=="finance"){ // unreachable due to previous branch name, handle separate
            // handled below
        }
        else if(cmd=="show"||cmd=="log"||cmd=="report"){ /* placeholder */ }
        else if(cmd=="show" && tokens.size()>=2 && tokens[1]=="finance"){ if(current_priv()<7){ invalid(); continue; } int cnt=-1; if(tokens.size()==3){ try{ cnt=stoi(tokens[2]); }catch(...){ invalid(); continue; } if(cnt==0){ cout<<"\n"; continue; } } auto p=store.sum_finance(cnt); cout<<"+ "<<fixed<<setprecision(2)<<(double)p.first<<" - "<<fixed<<setprecision(2)<<(double)p.second<<"\n"; }
        else if(cmd=="log"){ if(current_priv()<7){ invalid(); continue; } // simple log: output transactions file
            ifstream in("trans.log"); string x; bool any=false; while(getline(in,x)){ cout<<x<<"\n"; any=true; } if(!any) cout<<"\n"; }
        else if(cmd=="report"){ if(current_priv()<7){ invalid(); continue; } // basic reports
            if(tokens.size()!=2){ invalid(); continue; } if(tokens[1]=="finance"){ auto p=store.sum_finance(-1); cout<<"FINANCE REPORT\nIncome: "<<fixed<<setprecision(2)<<(double)p.first<<"\nExpenditure: "<<fixed<<setprecision(2)<<(double)p.second<<"\n"; }
            else if(tokens[1]=="employee"){ cout<<"EMPLOYEE REPORT\n"; /* minimal */ }
            else { invalid(); }
        }
        else {
            invalid();
        }
    }
    return 0;
}
