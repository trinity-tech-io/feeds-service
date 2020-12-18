#ifndef _FEEDS_AUTOUPDATE_HPP_
#define _FEEDS_AUTOUPDATE_HPP_

#include <memory>

namespace trinity {

class ThreadPool;

class AutoUpdate {
public:
    /*** type define ***/
    class Listener {
    public:

    protected:
        explicit Listener() = default;
        virtual ~Listener() = default;
    };

    /*** static function and variable ***/
    static std::shared_ptr<AutoUpdate> GetInstance();

    /*** class function and variable ***/

protected:
    /*** type define ***/

    /*** static function and variable ***/

    /*** class function and variable ***/

private:
    /*** type define ***/

    /*** static function and variable ***/
    static std::shared_ptr<AutoUpdate> AutoUpdateInstance;

    /*** class function and variable ***/
    explicit AutoUpdate() = default;
    virtual ~AutoUpdate() = default;
};

/***********************************************/
/***** class template function implement *******/
/***********************************************/

/***********************************************/
/***** macro definition ************************/
/***********************************************/

} // namespace trinity

#endif /* _FEEDS_AUTOUPDATE_HPP_ */


