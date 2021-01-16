#ifndef _FEEDS_ONE_DRIVE_HPP_
#define _FEEDS_ONE_DRIVE_HPP_

#include <CloudDrive.hpp>
#include <string>

namespace trinity {

class OneDrive : public CloudDrive {
public:
    /*** type define ***/

    /*** static function and variable ***/

    /*** class function and variable ***/
    virtual int makeDir(const std::string& dirName) override;

protected:
    /*** type define ***/

    /*** static function and variable ***/

    /*** class function and variable ***/

private:
    /*** type define ***/

    /*** static function and variable ***/

    /*** class function and variable ***/
    explicit OneDrive(const std::string& driveUrl, const std::string& accessToken);
    virtual ~OneDrive();

    std::string driveUrl;
    std::string accessToken;

    friend CloudDrive;
};

/***********************************************/
/***** class template function implement *******/
/***********************************************/

/***********************************************/
/***** macro definition ************************/
/***********************************************/

} // namespace trinity

#endif /* _FEEDS_ONE_DRIVE_HPP_ */


