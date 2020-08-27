08/16/2020 Fenxiang Li lifenxiang@trinity-tech.io

**version 1.0.1**, main changes to previous version:
- Add README.
- Add Disclaimer.
- Add Dockerfile.
- Minor fix.

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