// MIT No Attribution

// Copyright 2024 Dr Seb N.F. Sikora

// Permission is hereby granted, free of charge, to any person obtaining a copy of this
// software and associated documentation files (the "Software"), to deal in the Software
// without restriction, including without limitation the rights to use, copy, modify,
// merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
// PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
// HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
// SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


// Examples to demonstrate use of cpp-tp.hpp (scroll down to main())


// to compile, link against libpthread, eg:
// $ g++ -std=c++14 examples.cpp -o2 -DVERBOSE -o examples -lpthread


# include "cpp-tp.hpp"


#include <vector>
#include <iostream>
#include <memory>
#include <chrono>
#include <random>
#include <algorithm>
#include <numeric>
#include <functional>


const int MIN_WORK_DURATION_MSEC = 500;
const int MAX_WORK_DURATION_MSEC = 5000;


// Toy class with member functions that can do some work, and corresponding member functions
// that can be called to signal that the work is complete and return the corresponding return value.
class TestClass {
public:
    TestClass() { }

    struct ResultType {
        std::vector<int> result;
        int sum;
        bool even_sum;
    };
    
    int workFunc(std::vector<int> inputs) {
        randomThreadDelay(MIN_WORK_DURATION_MSEC, MAX_WORK_DURATION_MSEC);
        int acc = 0;
        for (int i : inputs) {
            acc += i;
        }
        return acc;
    }

    std::unique_ptr<ResultType> workFunc2(const std::vector<int> inputs) {
        randomThreadDelay(MIN_WORK_DURATION_MSEC, MAX_WORK_DURATION_MSEC);

        std::unique_ptr<ResultType> result = std::make_unique<ResultType>();

        result->result = inputs;
        std::reverse(result->result.begin(), result->result.end());
        
        result->sum = std::accumulate(result->result.begin(), result->result.end(), 0);
        result->even_sum = (result->sum % 2) ? false : true;

        return result;
    }

    void onCompletion(int result) {
        std::cout << "Result is " << result << std::endl;
    }

    void onCompletion2(std::unique_ptr<ResultType> result) {
        std::string msg = "Result - Reversed inputs:";
        for (int i : result->result) {
            msg += " " + std::to_string(i);
        }
        msg += "    Sum: " + std::to_string(result->sum);
        msg += (result->even_sum) ? " (Even)\n" : " (Odd)\n";
        std::cout << msg;
    }

    void randomThreadDelay(int min, int max) {
        std::mt19937_64 eng{std::random_device{}()};
        std::uniform_int_distribution<> dist{min, max};
        std::this_thread::sleep_for(std::chrono::milliseconds{dist(eng)});
    }
};


int main(int argc, char** argv) {

    auto tc = std::make_unique<TestClass>(TestClass());
    

    // Example 1.
    std::cout << std::endl << "Example 1." << std::endl << std::endl;

    // Create the threadpool.
    ThreadPool tp(true, 4);  // auto_start = true, worker_count = 4

    std::vector<int> arg_1({0, 0, 0, 0, 1});
    std::vector<int> arg_2({0, 0, 0, 1, 1});
    std::vector<int> arg_3({0, 0, 1, 1, 1});
    std::vector<int> arg_4({0, 1, 1, 1, 1});
    std::vector<int> arg_5({1, 1, 1, 1, 1});

    // Create some job callables...
    auto work_func_1 = [&tc, &arg_1](){return tc->onCompletion([&tc, arg_1](){ return tc->workFunc(arg_1); }());};
    auto work_func_2 = [&tc, &arg_2](){return tc->onCompletion([&tc, arg_2](){ return tc->workFunc(arg_2); }());};
    auto work_func_3 = [&tc, &arg_3](){return tc->onCompletion([&tc, arg_3](){ return tc->workFunc(arg_3); }());};
    auto work_func_4 = [&tc, &arg_4](){return tc->onCompletion([&tc, arg_4](){ return tc->workFunc(arg_4); }());};
    // capture argument by reference to avoid copying, but then we are responsible for ensuring the data
    // stays in scope!
    auto work_func_5 = [&tc, &arg_5](){return tc->onCompletion([&tc, &arg_5](){ return tc->workFunc(arg_5); }());};
    
    // ...and add them to the queue
    tp.addJob(work_func_1);
    tp.addJob(work_func_2);
    tp.addJob(work_func_3);
    tp.addJob(work_func_4);
    tp.addJob(work_func_5);
    
    tp.wait();  // wait for all pending jobs to complete


    // Example 2 - return a non pod result type by unique_ptr
    std::cout << std::endl << "Example 2." << std::endl << std::endl;

    // return value is never named, so the compiler can move it all the way to the destination
    auto work_func_6 = [&tc, &arg_1](){return tc->onCompletion2([&tc, &arg_1](){ return tc->workFunc2(arg_1); }());};
    auto work_func_7 = [&tc, &arg_2](){return tc->onCompletion2([&tc, &arg_2](){ return tc->workFunc2(arg_2); }());};
    auto work_func_8 = [&tc, &arg_3](){return tc->onCompletion2([&tc, &arg_3](){ return tc->workFunc2(arg_3); }());};
    auto work_func_9 = [&tc, &arg_4](){return tc->onCompletion2([&tc, &arg_4](){ return tc->workFunc2(arg_4); }());};
    auto work_func_10 = [&tc, &arg_5](){return tc->onCompletion2([&tc, &arg_5](){ return tc->workFunc2(arg_5); }());};
    
    tp.addJob(work_func_6);
    tp.addJob(work_func_7);
    tp.addJob(work_func_8);
    tp.addJob(work_func_9);
    tp.addJob(work_func_10);
    
    tp.wait();
    tp.stop();  // stop the threadpool, delete all threads
    // stop(bool clear_queue = true) will wait for all running jobs to complete, pending jobs
    // on the queue will be deleted unless clear_queue = false.

    std::cout << std::endl;

    return 0;
}
