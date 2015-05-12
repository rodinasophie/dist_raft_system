#ifndef __COUNTED_PTR_HPP_
#define __COUNTED_PTR_HPP_

template <class X>
class counted_ptr {
 public:
	typedef X element_type;
	explicit counted_ptr(X* p = 0) // allocate a new counter
		: itsCounter(0) {if (p) itsCounter = new counter(p);}
	~counted_ptr() {
		release();
	}
	counted_ptr(const counted_ptr& r) throw() {
		acquire(r.itsCounter);
	}
	counted_ptr& operator=(const counted_ptr& r) {
		if (this != &r) {
			release();
			acquire(r.itsCounter);
		}
	  return *this;
	}


																		    X& operator*()  const throw()   {return *itsCounter->ptr;}
																				    X* operator->() const throw()   {return itsCounter->ptr;}
																						    X* get()        const throw()   {return itsCounter ? itsCounter->ptr : 0;}
																								    bool unique()   const throw()
																											        {return (itsCounter ? itsCounter->count == 1 : true);}

	private:

																										    struct counter {
																													        counter(X* p = 0, unsigned c = 1) : ptr(p), count(c) {}
																																	        X*          ptr;
																																					        unsigned    count;
																																									    }* itsCounter;

																												    void acquire(counter* c) throw()
																															    { // increment the count
																																		        itsCounter = c;
																																						        if (c) ++c->count;
																																										    }

																														    void release()
																																	    { // decrement the count, delete if it is 0
																																				        if (itsCounter) {
																																									            if (--itsCounter->count == 0) {
																																																                delete itsCounter->ptr;
																																																								                delete itsCounter;
																																																																            }
																																															            itsCounter = 0;
																																																					        }
																																								    }
};
#endif // __COUNTED_PTR_HPP_
