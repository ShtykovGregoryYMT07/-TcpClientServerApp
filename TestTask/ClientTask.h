#ifndef CLIENTTASK_H
#define CLIENTTASK_H

#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>


using namespace boost::archive;

class ClientTask
{
public:
    ClientTask() = default;
    ClientTask(double lowerBound, double upperBound, double step, int usingThreads, int methodType)
        : locLowerBound(lowerBound), locUpperBound(upperBound), step(step), locUsingThreds(usingThreads), typeUseMethods(methodType) {}

 ClientTask(const ClientTask& other)
        : locLowerBound(other.locLowerBound),
          locUpperBound(other.locUpperBound),
          step(other.step),
          locUsingThreds(other.locUsingThreds),
          typeUseMethods(other.typeUseMethods)
    {
    }

    ClientTask& operator=(const ClientTask& other)
    {
        if (this != &other) {
            locLowerBound = other.locLowerBound;
            locUpperBound = other.locUpperBound;
            step = other.step;
            locUsingThreds = other.locUsingThreds;
            typeUseMethods = other.typeUseMethods;
        }
        return *this;
    }    

    double getLowerBound() const { return locLowerBound; }
    void setLowerBound(double lowerBound) { locLowerBound = lowerBound; }

    double getUpperBound() const { return locUpperBound; }
    void setUpperBound(double upperBound) { locUpperBound = upperBound; }

    double getStep() const { return step; }
    void setStep(double newStep) { step = newStep; }

    int getUsingThreads() const { return locUsingThreds; }
    void setUsingThreads(int threads) { locUsingThreds = threads; }

    int getTypeUseMethods() const { return typeUseMethods; }
    void setTypeUseMethods(int methodType) { typeUseMethods = methodType; }

private:
    // Данные задания для клиента
    double locLowerBound;
    double locUpperBound;
    double step;
    int locUsingThreds;
    int typeUseMethods;

    // Декларация для сериализации на основе Boost
    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive &ar, const unsigned int version)
    {
        ar & locLowerBound;
        ar & locUpperBound;
        ar & step;
        ar & locUsingThreds;
        ar & typeUseMethods;
    }
};


#endif // CLIENTTASK_H
