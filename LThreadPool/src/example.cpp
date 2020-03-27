#include <ctpl.h>
#include <iostream>
#include <string>

ctpl::ThreadPool p(2 /* two threads in the pool */);

void first(int id) {
    std::cout << "hello from " << id << ", function\n";
}

void aga(int id, int par) {
    std::cout << "hello from " << id << ", function with parameter " << par <<'\n';
}

struct Third {
    Third(int v) { this->v = v; std::cout << "Third ctor " << this->v << '\n'; }
    Third(Third && c) { this->v = c.v; std::cout<<"Third move ctor\n"; }
    Third(const Third & c) { this->v = c.v; std::cout<<"Third copy ctor\n"; }
    ~Third() { std::cout << "Third dtor\n"; }
    int v;
};

void mmm(int id, const std::string & s) {
    std::cout << "mmm function " << id << ' ' << s << '\n';
}

void ugu(int id, Third & t) {
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    std::cout << "hello from " << id << ", function with parameter Third " << t.v <<'\n';
}

void lzw_test1() {
    std::cout << "------------------------- lzw_test1 -------------------------" << std::endl;
    std::future<void> qw = p.Push(std::ref(first));  // function
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    p.Push(first);  // function
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    p.Push(aga, 7);  // function
    std::cout << "------------------------- lzw_test1 -------------------------" << std::endl;
}

void lzw_test2() {
    struct Second {
        Second(const std::string & s) { std::cout << "Second ctor\n"; this->s = s; }
        Second(Second && c) { std::cout << "Second move ctor\n"; s = std::move(c.s); }
        Second(const Second & c) { std::cout << "Second copy ctor\n"; this->s = c.s; }
        ~Second() { std::cout << "Second dtor\n"; }
        void operator()(int id) const {
            std::cout << "hello from " << id << ' ' << this->s << '\n';
        }
    private:
        std::string s;

    } second(", functor");
    p.Push(std::ref(second));  // functor, reference
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    p.Push(const_cast<const Second &>(second));  // functor, copy ctor
    p.Push(std::move(second));  // functor, move ctor
    p.Push(second);  // functor, move ctor
    p.Push(Second(", functor"));  // functor, move ctor
}

void lzw_test3() {
    {
        Third t(100);

        p.Push(ugu, std::ref(t));  // function. reference
        p.Push(ugu, t);  // function. copy ctor, move ctor
        p.Push(ugu, std::move(t));  // function. move ctor, move ctor

    }
    p.Push(ugu, Third(200));  // function
}

void lzw_test4() {
    std::string s = ", lambda";
    p.Push([s](int id){  // lambda
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        std::cout << "hello from " << id << ' ' << s << '\n';
    });

    p.Push([s](int id){  // lambda
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        std::cout << "hello from " << id << ' ' << s << '\n';
    });
}

void lzw_test5() {
    p.Push(mmm, "worked");

    auto f = p.Pop();
    if (f) {
        std::cout << "Poped function from the pool ";
        // f(0);
    }
    // change the number of treads in the pool

    p.Resize(1);

    std::string s2 = "result";
    auto f1 = p.Push([s2](int){
        return s2;
    });
    // other code here
    //...
    std::cout << "returned " << f1.get() << '\n';
}

void lzw_test6() {    
    auto f2 = p.Push([](int){
        throw std::exception();
    });
    // other code here
    //...
    try {
        f2.get();
    }
    catch (std::exception & e) {
        std::cout << "caught exception\n";
    }

    // get thread 0
    // auto & th = p.get_thread(0);

}

void lzw_test7() {
    
}

void lzw_test8() {
    
}


int main(int argc, char **argv) {
    
    lzw_test1();
    // lzw_test2();
    // lzw_test3();
    // lzw_test4();
    // lzw_test5();
    // lzw_test6();   

    return 0;
}
