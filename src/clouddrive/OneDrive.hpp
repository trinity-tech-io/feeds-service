#ifndef _FEEDS_ONE_DRIVE_HPP_
#define _FEEDS_ONE_DRIVE_HPP_

#include <CloudDrive.hpp>
#include <string>

namespace trinity {

class HttpClient;

class OneDrive : public CloudDrive {
public:
    /*** type define ***/

    /*** static function and variable ***/

    /*** class function and variable ***/
    virtual int makeDir(const std::string& dirPath) override;
    virtual int remove(const std::string& filePath) override;
    virtual int write(const std::string& filePath, std::shared_ptr<std::istream> content) override;

protected:
    /*** type define ***/

    /*** static function and variable ***/

    /*** class function and variable ***/

private:
    /*** type define ***/

    /*** static function and variable ***/

    /*** class function and variable ***/
    explicit OneDrive(const std::string& driveUrl,
                      const std::string& driveRootDir,
                      const std::string& accessToken);
    virtual ~OneDrive();

    int makeHttpClient(const std::string& url, HttpClient& httpClient);
    int touchFile(const std::string& filePath, std::string& fileUrl);
    int uploadFile(const std::string& fileUrl, std::shared_ptr<std::istream> content);

    std::string driveUrl;
    std::string driveRootDir;
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


