#ifndef _FEEDS_CLOUD_DRIVE_HPP_
#define _FEEDS_CLOUD_DRIVE_HPP_

#include <memory>

namespace trinity {

class CloudDrive {
public:
    /*** type define ***/
    enum Type {
        OneDrive,
    };

    /*** static function and variable ***/
    static std::shared_ptr<CloudDrive> Create(Type type,
                                              const std::string& driveUrl,
                                              const std::string& accessToken);

    /*** class function and variable ***/
    virtual int makeDir(const std::string& dirName) = 0;

protected:
    /*** type define ***/

    /*** static function and variable ***/

    /*** class function and variable ***/
    explicit CloudDrive() = default;
    virtual ~CloudDrive() = default;

private:
    /*** type define ***/

    /*** static function and variable ***/

    /*** class function and variable ***/

};

/***********************************************/
/***** class template function implement *******/
/***********************************************/

/***********************************************/
/***** macro definition ************************/
/***********************************************/

} // namespace trinity

#endif /* _FEEDS_CLOUD_DRIVE_HPP_ */


