13/01/2021 XiaokunMeng mengxiaokun@trinity-tech.io

**version 1.5.0**, main changes to previous version:
- update libcrystal to 1.0.6.
- Update carrier to 6.0.2.
- fix get_multi_likes_and_comments_count response issue.
- add -v/--version argument.
- update version to x.x.x(git-commit).
- Add depends on libressl and libcurl.
- Add get_multi_likes_and_comments_count and get_multi_subscribers_count api.
- Add C file module prefix to log.
- Add module prefix to log.

14/12/2020 XiaokunMeng mengxiaokun@trinity-tech.io

**version 1.4.0**, main changes to previous version:
- Fix same thread conflic issue.
- Forward compatible old login.
- Fix exit crash issue when session is transfering.
- Fix same memory leak issue.
- Add feeds service info to challage.
- Add version_code param to GetVersion api.
- Update Elastos.DID.Native.SDK to branch release-v1.x.
- Optimize log output.
- Optimize async send message by carrier.
- Fix message ref crash issue.
- Use msgpack to format request/response object print info.
- Add get_multi_comments rpc.
- Change msgpack to c++.
- Integrate SQLite for cpp.
- Fix compatibility issue when use access token to do rpc.
- Remove depends on json.
- Fix origin login check challage issue.
- Fix instance did verify.
- Add standard_did_auth api.
- Add standard_sign_in api.
- Update Elastos.DID.Native.SDK to v1.2.

21/11/2020 XiaokunMeng mengxiaokun@trinity-tech.io

**version 1.3.0**, main changes to previous version:
- Fix RaspberryPi build issue.
- Add cmake build version with git commit-id.
- Add build version to commit-id.
- Process command handler with asynchronous.
- Move Legacy message process to command handler.
- Add set_bindata and get_bindata throught message channel.
- Fix get_posts rpc issue on status decleared.
- Fix session release issue when carrier disconnected.
- Add new post RPC: declare_post, notify_post.
- Add block comment RPC.
- Add report_illegal_comment RPC.
- Add db table to save reported_comments.
- Modify session protocol MaginNumber to adapte JS.
- Add package resources on Darwin.
- Fix RPC service to update Feeds source bug.
- Fix build issue on Ubuntu.
- Improve RPC service to update Feeds source (independent APIs).
- Support to retrieve the total  number of DIDs connected with that Feeds source.
- Add get_service_version RPC.
- Support to use thumbnail as index to retrieve the real picture and video.
- Add set_binary and get_binary RPC.
- Add carrier session wrapper.
- Updated readme document.

09/11/2020 Fenxiang Li lifenxiang@trinity-tech.io

**version 1.2.0**, main changes to previous version:
- Update README.md.
- Not update upd_at field of channel when including posts and comments changed.
- Support post edit and deletion.
- Support comment edit and deletion.

08/27/2020 Fenxiang Li lifenxiang@trinity-tech.io

**version 1.1.0**, main changes to previous version:
- Set raspberry pi default listen ip to 0.0.0.0.
- Fix printing for guiding user to visit http service.
- Fix mac listen ip not set.
- Print log if http server ip:port is in use.
- Add new error type.
- Fix multi-device notification.
- Support service credential modification.
- Support channel update/update notification.
- Redefine updated_at column in db to the update time of the row itself.
- Respond success to enable_notification request even if it has already enabled.
- Respond to unsupported request.
- Set http server listen ip to 0.0.0.0.
- Print owner did and feeds did on startup.
- Clarify the responsibility of releasing marshalled data.

08/16/2020 Fenxiang Li lifenxiang@trinity-tech.io

**version 1.0.1**, main changes to previous version:
- Add README.
- Add Disclaimer.
- Add Dockerfile.
- Minor fix.
