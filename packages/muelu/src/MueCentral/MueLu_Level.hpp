#ifndef MUELU_LEVEL_HPP
#define MUELU_LEVEL_HPP

#include <iostream>
#include <sstream>

#include "MueLu_ConfigDefs.hpp"
#include "MueLu_Exceptions.hpp"
#include "MueLu_Needs.hpp"
#include "MueLu_FactoryBase.hpp"
#include "MueLu_DefaultFactoryHandlerBase.hpp"

namespace MueLu {

/*!
    @class Level
    @brief Class that holds all level-specific information.

    This class stores <tt>A</tt>, <tt>R</tt>, <tt>P</tt>, the presmother and the postsmoother
    explicitly.  All other data is stored in an associative list.
    See the Needs class for more information.
 */
class Level : public Needs {

private:
    mutable int levelID_; // id number associated with level
    RCP<DefaultFactoryHandlerBase> defaultFactoryHandler_;

    // linked list of Level
    RCP<Level> previousLevel_;

protected:
    RCP<Teuchos::FancyOStream> out_;

public:

    //@{
    //! @name Constructors / Destructors
    Level() : levelID_(-1), out_(this->getOStream()) {
        //Teuchos::OSTab tab(out_); MueLu_cout(Teuchos::VERB_HIGH) << "Instantiating new uninitialized Level" << std::endl;
    }

    //! Constructor
    Level(RCP<DefaultFactoryHandlerBase>& defaultFactoryHandler) : levelID_(-1), defaultFactoryHandler_(defaultFactoryHandler), out_(this->getOStream()) { }

    //! Copy constructor.
    explicit Level(const Level& source) {
        levelID_ = source.levelID_;
        defaultFactoryHandler_ = source.defaultFactoryHandler_;
    }

    //@}

    //@{
    //! @name Build methods //TODO: merge with copy constructor?
    //! Builds a new Level object.
    RCP<Level> Build(std::ostream &os) { //TODO: why ostream in argument?
        return rcp( new Level(defaultFactoryHandler_) );
    }
    //@}

    virtual ~Level() {}

    void Print(std::ostream &os) {
        os << this << std::endl;
    }

    //@{
    //! @name Set methods.

    //! @brief Set level number.
    void SetLevelID(int i) const {
        levelID_ = i;
    }

    void SetPreviousLevel(const RCP<Level> & previousLevel) {
        previousLevel_ = previousLevel;
    }

    //! Set default factories (used internally by Hierarchy::SetLevel()).
    // Users should not use this method.
    void SetDefaultFactoryHandler(RCP<DefaultFactoryHandlerBase>& defaultFactoryHandler) {
        defaultFactoryHandler_ = defaultFactoryHandler;
    }

    //@}

    //@{
    //! @name Set/Get methods.

    // TODO REMOVE ME
    //! Store need label and its associated data. This does not increment the storage counter.
    template <class T>
    void Set(const std::string ename, const T &entry, const FactoryBase* factory) {
      //      TEST_FOR_EXCEPTION(not null, or reference instead);
        Needs::SetData<T>(ename, entry, factory);
    } //Set

    //! Store need label and its associated data. This does not increment the storage counter.
    template <class T>
    void Set(const std::string& ename, const T &entry, Teuchos::RCP<const FactoryBase> factory) {
        Needs::SetData<T>(ename, entry, factory.get());
    }

    //! Store need label and its associated data. This does not increment the storage counter.
    /* */
    template <class T>
    void Set(const std::string& ename, const T &entry) {
        Needs::SetData<T>(ename, entry, NULL);  // no factory
    }

    /*! @brief Get data without decrementing associated storage counter (i.e., read-only access). */
    // Usage: Level->Get< RCP<Operator> >("A", factory)
    // factory == NULL => use default factory
    template <class T>
    T & Get(const std::string& ename, const FactoryBase* factory)
    {
        // variable 'ename' generated by 'factory' available in Level
        if (  Needs::IsAvailable(ename, factory) )
        {
            // default: data already available
            return Needs::GetData<T>(ename,factory);
        }

        // data not available
        // if no generating factory given, use DefaultFactoryHandler
        if (factory == NULL)
        {
            const FactoryBase* defaultFactory = GetDefaultFactoryPtr(ename);
            // check if data for default factory has already been generated
            if(!Needs::IsAvailable(ename,defaultFactory))
            {
                // todo: call declareInput!
                defaultFactory->NewBuild(*this);
            }

            TEST_FOR_EXCEPTION(! Needs::IsAvailable(ename,defaultFactory), Exceptions::RuntimeError, "MueLu::Level::Get(): factory did not produce expected output.");
            return Needs::GetData<T>(ename,defaultFactory);
        }
        else
        {
            // todo: call declareInput?
            factory->NewBuild(*this);
            TEST_FOR_EXCEPTION(! Needs::IsAvailable(ename,factory), Exceptions::RuntimeError, "MueLu::Level::Get(): factory did not produce expected output.");
            return Needs::GetData<T>(ename,factory);
        }
    }

    /*! @brief Get data without decrementing associated storage counter (i.e., read-only access). */
    // Usage: Level->Get< RCP<Operator> >("A", factory)
    // factory == Teuchos::null => use default factory
    template <class T>
    T & Get(const std::string& ename, Teuchos::RCP<const FactoryBase> factory)
    {
        return Get<T>(ename,factory.get());
    }

    /*! @brief Get data without decrementing associated storage counter (i.e., read-only access).
     * no generating factory available and no default factory
     * */
    template <class T>
    T & Get(const std::string& ename)
    {
        return Get<T>(ename,NULL);
    }

    /*! @brief Get data without decrementing associated storage counter (i.e., read-only access).
     * no generating factory available and no default factory
     * */
    template <class T>
    void Get(const std::string& ename, T& Value, Teuchos::RCP<const FactoryBase> factory)
    {
        Value = Get<T>(ename,factory.get());
    }

    /*! @brief Get data without decrementing associated storage counter (i.e., read-only access).
     * no generating factory available and no default factory
     * */
    template <class T>
    void Get(const std::string& ename, T& Value)
    {
        Value = Get<T>(ename,NULL);
    }

    //! @brief Return level number.
    int GetLevelID() const { return levelID_; }

    //!
    void Input(const std::string& ename, const FactoryBase* factory) {
        //TODO
    }

    //! Previous level
    RCP<Level> & GetPreviousLevel() { return previousLevel_; }
    //@}


    void Input(const std::string& ename, Teuchos::Ptr<const FactoryBase> factory) {
        Input(ename, factory.get());
    }


private:

    //! Get default factory.
    const FactoryBase & GetDefaultFactory(const std::string& varname) {
        TEST_FOR_EXCEPTION(defaultFactoryHandler_ == null, Exceptions::RuntimeError, "MueLu::Level::GetDefaultFactory(): no DefaultFactoryHandler.");
        return defaultFactoryHandler_->GetDefaultFactory(varname);
    }

    //! Get ptr to default factory.
    const FactoryBase* GetDefaultFactoryPtr(const std::string& varname) {
        TEST_FOR_EXCEPTION(defaultFactoryHandler_ == null, Exceptions::RuntimeError, "MueLu::Level::GetDefaultFactory(): no DefaultFactoryHandler.");
        return defaultFactoryHandler_->GetDefaultFactoryRCP(varname).get();
    }


}; //class Level

std::ostream& operator<<(std::ostream& os, Level const &level);

} //namespace MueLu

#define MUELU_LEVEL_SHORT
#endif //ifndef MUELU_LEVEL_HPP
