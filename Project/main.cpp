#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <cmath>
#include <iomanip>

using namespace std;

// --- UTILITIES ---
string readFile(const string& filename) {
    ifstream file(filename);
    if (!file.is_open()) return "";
    stringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

string toLower(string s) {
    transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s;
}

string escapeJSON(const string& s) {
    string result;
    for (char c : s) {
        if (c == '"') result += "\\\"";
        else if (c == '\\') result += "\\\\";
        else if (c == '\n' || c == '\r' || c == '\t') result += " ";
        else result += c;
    }
    return result;
}

vector<string> splitSentences(const string& text) {
    vector<string> sentences;
    string current;
    for (size_t i = 0; i < text.size(); i++) {
        current += text[i];
        if ((text[i] == '.' || text[i] == '!' || text[i] == '?') && current.size() > 25) {
            size_t start = current.find_first_not_of(" \t\n");
            if (start != string::npos) sentences.push_back(current.substr(start));
            current.clear();
        }
    }
    return sentences;
}

set<string> getStopwords() {
    return {"the","a","an","and","or","but","in","on","at","to","for","of","with",
            "by","from","is","are","was","were","be","been","have","has","had",
            "this","that","it","which","who","what","where","when","how","all", "not", "no"};
}

// --- ALGORITHMS ---
vector<int> computeLPS(const string& pat) {
    int m = pat.length();
    vector<int> lps(m, 0);
    int len = 0, i = 1;
    while (i < m) {
        if (pat[i] == pat[len]) { len++; lps[i] = len; i++; }
        else {
            if (len != 0) len = lps[len - 1];
            else { lps[i] = 0; i++; }
        }
    }
    return lps;
}

bool kmpSearch(const string& txt, const string& pat) {
    string lowerTxt = toLower(txt);
    string lowerPat = toLower(pat);
    int n = lowerTxt.length(), m = lowerPat.length();
    if (m == 0) return false;
    vector<int> lps = computeLPS(lowerPat);
    int i = 0, j = 0;
    while (i < n) {
        if (lowerPat[j] == lowerTxt[i]) { j++; i++; }
        if (j == m) return true;
        else if (i < n && lowerPat[j] != lowerTxt[i]) {
            if (j != 0) j = lps[j - 1];
            else i++;
        }
    }
    return false;
}

map<string, double> tfVector(const string& sentence) {
    set<string> stopwords = getStopwords();
    map<string, double> vec;
    stringstream ss(sentence);
    string word;
    while (ss >> word) {
        string clean;
        for (char c : word) if (isalpha(c)) clean += tolower(c);
        if (clean.size() > 2 && stopwords.find(clean) == stopwords.end()) vec[clean]++;
    }
    return vec;
}

double cosineSim(const map<string,double>& a, const map<string,double>& b) {
    double dot = 0, normA = 0, normB = 0;
    for (auto& p : a) { normA += p.second * p.second; if (b.count(p.first)) dot += p.second * b.at(p.first); }
    for (auto& p : b) normB += p.second * p.second;
    if (normA == 0 || normB == 0) return 0.0;
    return dot / (sqrt(normA) * sqrt(normB));
}

int main() {
    string text = readFile("raw_data.txt");
    if (text.empty()) return 1;

    vector<string> sentences = splitSentences(text);
    int totalSentences = sentences.size();
    if(totalSentences == 0) return 1;

    ofstream out("results.json");
    out << "{\n";

    // 1. TF-IDF
    out << "  \"keywords\": [\n";
    set<string> stopwords = getStopwords();
    map<string, int> tf;
    int totalWords = 0;
    stringstream ss(text);
    string word;
    while(ss >> word) {
        string clean;
        for(char c : word) if(isalpha(c)) clean += tolower(c);
        if(clean.size() > 2 && stopwords.find(clean) == stopwords.end()) { tf[clean]++; totalWords++; }
    }
    vector<pair<string, int>> sorted_tf(tf.begin(), tf.end());
    sort(sorted_tf.begin(), sorted_tf.end(), [](const pair<string,int>& a, const pair<string,int>& b){ return a.second > b.second; });
    
    string topKeyword = sorted_tf.empty() ? "" : sorted_tf[0].first;

    int kwCount = 0;
    for (auto& p : sorted_tf) {
        if (kwCount >= 8) break;
        double score = (double)p.second / totalWords * 100.0;
        out << "    { \"word\": \"" << escapeJSON(p.first) << "\", \"score\": \"" << fixed << setprecision(2) << score << "\" }";
        if (kwCount < 7 && kwCount < sorted_tf.size() - 1) out << ",";
        out << "\n";
        kwCount++;
    }
    out << "  ],\n";

    // 2. KMP SMART SEARCH
    out << "  \"kmp_context\": [\n";
    int kmpHits = 0;
    for (const string& s : sentences) {
        if (kmpHits >= 3) break; 
        if (kmpSearch(s, topKeyword)) {
            out << "    \"" << escapeJSON(s) << "\"";
            if (kmpHits < 2) out << ",\n"; else out << "\n";
            kmpHits++;
        }
    }
    if (kmpHits < 3 && kmpHits > 0) {
        long pos = out.tellp();
        out.seekp(pos - 2);
        out << "\n";
    }
    out << "  ],\n";

    int removed = 0;
    int limit = min((int)sentences.size(), 200); 
    vector<bool> keep(limit, true);
    for (int i = 0; i < limit; i++) {
        if (!keep[i]) continue;
        auto vi = tfVector(sentences[i]);
        for (int j = i + 1; j < limit; j++) {
            if (!keep[j]) continue;
            auto vj = tfVector(sentences[j]);
            if (cosineSim(vi, vj) >= 0.80) { keep[j] = false; removed++; }
        }
    }
    
    out << "  \"stats\": {\n";
    out << "    \"total_sentences\": " << totalSentences << ",\n";
    out << "    \"dupes_removed\": " << removed << ",\n";
    out << "    \"top_keyword\": \"" << escapeJSON(topKeyword) << "\"\n";
    out << "  }\n";
    out << "}\n";
    
    out.close();
    return 0;
}