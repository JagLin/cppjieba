#ifndef CPPJIEBA_KEYWORD_EXTRACTOR_H
#define CPPJIEBA_KEYWORD_EXTRACTOR_H

#include <cmath>
#include <set>
#include "MixSegment.hpp"

namespace cppjieba {

using namespace limonp;
using namespace std;

/*utf8*/
class KeywordExtractor {
 public:
  struct Word {
    string word;
    string pos;
    vector<size_t> offsets;
    double weight;
  }; // struct Word

  KeywordExtractor(const string& dictPath,
        const string& hmmFilePath,
        const string& idfPath,
        const string& stopWordPath,
        const string& userDict = "")
    : segment_(dictPath, hmmFilePath, userDict) {
    LoadIdfDict(idfPath);
    LoadStopWordDict(stopWordPath);
  }
  KeywordExtractor(const DictTrie* dictTrie,
        const HMMModel* model,
        const string& idfPath,
        const string& stopWordPath)
    : segment_(dictTrie, model) {
    LoadIdfDict(idfPath);
    LoadStopWordDict(stopWordPath);
  }
  ~KeywordExtractor() {
  }

  void Extract(const string& sentence, vector<string>& keywords, size_t topN) const {
    vector<Word> topWords;
    Extract(sentence, topWords, topN);
    for (size_t i = 0; i < topWords.size(); i++) {
      keywords.push_back(topWords[i].word);
    }
  }

  void Extract(const string& sentence, vector<pair<string, double> >& keywords, size_t topN) const {
    vector<Word> topWords;
    Extract(sentence, topWords, topN);
    for (size_t i = 0; i < topWords.size(); i++) {
      keywords.push_back(pair<string, double>(topWords[i].word, topWords[i].weight));
    }
  }

  void Extract(const string& sentence, vector<Word>& keywords, size_t topN) const {
    vector<pair<string, string> > tagResult;
    segment_.Tag(sentence,tagResult);
    for(size_t i=0;i<tagResult.size();++i){
      //dirty solution for pos tagging module...
      trim(tagResult[i].second);
      //XLOG(DEBUG) << tagResult[i].first << ":" << tagResult[i].second << endl;
    }

    // should be parameterized
    set<string> posSet;
    posSet.insert("n");
    posSet.insert("v");
    posSet.insert("a");
    posSet.insert("nr");
    //posSet.insert("eng");

    map<string, Word> wordmap;
    size_t offset = 0;
    for (size_t i = 0; i < tagResult.size(); ++i) {
      size_t t = offset;
      offset += tagResult[i].first.size();
      bool notFoundPos = !(std::find(posSet.begin(), posSet.end(), tagResult[i].second) != posSet.end());
      if (IsSingleWord(tagResult[i].first) || stopWords_.find(tagResult[i].first) != stopWords_.end() || notFoundPos) {
        continue;
      }
      wordmap[tagResult[i].first].offsets.push_back(t);
      wordmap[tagResult[i].first].weight += 1.0;
      wordmap[tagResult[i].first].pos = tagResult[i].second;
    }
    if (offset != sentence.size()) {
      XLOG(ERROR) << "words illegal";
      return;
    }

    keywords.clear();
    keywords.reserve(wordmap.size());
    for (map<string, Word>::iterator itr = wordmap.begin(); itr != wordmap.end(); ++itr) {
      unordered_map<string, double>::const_iterator cit = idfMap_.find(itr->first);
      if (cit != idfMap_.end()) {
        itr->second.weight *= cit->second;
      } else {
        itr->second.weight *= idfAverage_;
      }
      itr->second.word = itr->first;
      keywords.push_back(itr->second);
    }
    topN = min(topN, keywords.size());
    partial_sort(keywords.begin(), keywords.begin() + topN, keywords.end(), Compare);
    keywords.resize(topN);
  }
private:
  void trim(string& s, const string& delimiters = " \f\n\r\t\v" ) const {
      s.erase( s.find_last_not_of( delimiters ) + 1 ).erase( 0, s.erase( s.find_last_not_of( delimiters ) + 1 ).find_first_not_of( delimiters ) );
  }
  void LoadIdfDict(const string& idfPath) {
    ifstream ifs(idfPath.c_str());
    XCHECK(ifs.is_open()) << "open " << idfPath << " failed";
    string line ;
    vector<string> buf;
    double idf = 0.0;
    double idfSum = 0.0;
    size_t lineno = 0;
    for (; getline(ifs, line); lineno++) {
      buf.clear();
      if (line.empty()) {
        XLOG(ERROR) << "lineno: " << lineno << " empty. skipped.";
        continue;
      }
      Split(line, buf, " ");
      if (buf.size() != 2) {
        XLOG(ERROR) << "line: " << line << ", lineno: " << lineno << " empty. skipped.";
        continue;
      }
      idf = atof(buf[1].c_str());
      idfMap_[buf[0]] = idf;
      idfSum += idf;

    }

    assert(lineno);
    idfAverage_ = idfSum / lineno;
    assert(idfAverage_ > 0.0);
  }
  void LoadStopWordDict(const string& filePath) {
    ifstream ifs(filePath.c_str());
    XCHECK(ifs.is_open()) << "open " << filePath << " failed";
    string line ;
    while (getline(ifs, line)) {
      stopWords_.insert(line);
    }
    assert(stopWords_.size());
  }

  static bool Compare(const Word& lhs, const Word& rhs) {
    return lhs.weight > rhs.weight;
  }

  MixSegment segment_;
  unordered_map<string, double> idfMap_;
  double idfAverage_;

  unordered_set<string> stopWords_;
}; // class KeywordExtractor

inline ostream& operator << (ostream& os, const KeywordExtractor::Word& word) {
  return os << "{\"word\": \"" << word.word << "\", \"offset\": " << word.offsets << ", \"weight\": " << word.weight << ", \"pos\": " << word.pos << "}";
}

} // namespace cppjieba

#endif


