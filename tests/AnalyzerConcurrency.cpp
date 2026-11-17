#include "Analyzer.h"
#include <atomic>
#include <iostream>
#include <memory>
#include <thread>
#if defined(__unix__) || defined(__APPLE__)
#include <pthread.h>
#endif
#if defined(__unix__) || defined(__APPLE__)
namespace {
struct SmallStackCase {
    bassdeamon::AnalysisQueue* queue;
    bassdeamon::RelationAnalyzer* analyzer;
    bool recovered=false;
};

void* exerciseAnalyzerReset(void* context) {
    auto& test=*static_cast<SmallStackCase*>(context);
    auto& queue=*test.queue;
    auto& analyzer=*test.analyzer;
    analyzer.prepare(44100);
    for (uint32_t n=0;n<queue.capacity;++n) queue.push({});
    queue.push({}); // Deliberate overrun creates a gap in sequence numbers.
    analyzer.consume(queue);
    queue.push({});
    analyzer.consume(queue);
    test.recovered=analyzer.discontinuities==1 && !analyzer.ready();
    analyzer.prepare(96000);
    return nullptr;
}

bool resetWithMacWorkerStack() {
    auto queue=std::make_unique<bassdeamon::AnalysisQueue>();
    auto analyzer=std::make_unique<bassdeamon::RelationAnalyzer>();
    SmallStackCase test{queue.get(),analyzer.get()};
    pthread_attr_t attributes;
    if (pthread_attr_init(&attributes)!=0) return false;
    constexpr size_t stackBytes=512*1024;
    pthread_t worker;
    const bool created=pthread_attr_setstacksize(&attributes,stackBytes)==0 &&
        pthread_create(&worker,&attributes,exerciseAnalyzerReset,&test)==0;
    pthread_attr_destroy(&attributes);
    return created && pthread_join(worker,nullptr)==0 && test.recovered;
}
}
#endif
int main(){
#if defined(__unix__) || defined(__APPLE__)
    if (!resetWithMacWorkerStack()) {
        std::cerr<<"Analyzer reset failed on a 512 KiB worker stack\n";
        return 5;
    }
#endif
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
