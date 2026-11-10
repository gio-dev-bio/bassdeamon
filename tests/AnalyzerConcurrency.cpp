#include "Analyzer.h"
#include <atomic>
#include <iostream>
#include <memory>
#include <thread>
int main(){
    auto queue=std::make_unique<bassdeamon::AnalysisQueue>();
    auto analyzer=std::make_unique<bassdeamon::RelationAnalyzer>();analyzer->prepare(48000);
    for(uint32_t i=0;i<queue->capacity;++i)if(!queue->push({}))return 1;
    if(queue->pending()!=queue->capacity)return 2;
    std::atomic<bool> done{false},started{false};
    std::thread producer([&]{started.store(true);while(!done.load())queue->push({});});
    while(!started.load())std::this_thread::yield();
    // Keep the producer running until consume RETURNS. The former drain-until-
    // empty loop could hang indefinitely with an offline producer.
    analyzer->consume(*queue);
    done.store(true);producer.join();
    if(analyzer->streamSequence()!=queue->capacity){std::cerr<<"Consumed past the initial queue snapshot\n";return 3;}
    if(queue->pending()>queue->capacity)return 4;
    std::cout<<"PASS bounded UI consumer under continuous offline production; first update consumes exactly "<<queue->capacity<<" initial frames\n";
}
