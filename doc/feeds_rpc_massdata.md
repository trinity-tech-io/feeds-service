# Mass data RPC

## Set binary
```YAML
Request:
type set_binary_request = {
  "version": "1.0",
  "method": "set_binary",
  "id": 2,
  "params": {
    "access_token": "eyJhbGciOiAiRVMyNTYiLCAia2lkIjogImRpZDplbGFzdG9zOmlxSndzblJCZTZXUlFFYU1USndTYXFNYnZiOTUyWDhWQlMjcHJpbWFyeSJ9.eyJpc3MiOiAiZGlkOmVsYXN0b3M6aXFKd3NuUkJlNldSUUVhTVRKd1NhcU1idmI5NTJYOFZCUyIsICJzdWIiOiAiZGlkOmVsYXN0b3M6aW1aZ0FvOVczOFZ6bzFwSlFmSHA2TkpwOUxac3JuUlBSciIsICJleHAiOiAxNjEwNjc5MDgyLCAidWlkIjogMSwgIm5hbWUiOiAidGVzdCIsICJlbWFpbCI6ICJOQSJ9.dY1N2415mP4A8jTrFaeEGp8dzZHQ4D3FUNWr3Wm75b3x0VxI-TDZGKT72v84VyAGKFJZVYb4CtKbl86DWGMRZw",
    "key": "B4JEjs9vQFmsmXZ8ZiMaZkGGLBctQod2a8LVatULqijD-1-1-0-video-0",
    "algo": "None",
    "checksum": "",
    "content": Any
  }
}
Response:
type set_binary_response = {
  "version": "1.0",
  "id": 2,
  "result": {
    "key": "B4JEjs9vQFmsmXZ8ZiMaZkGGLBctQod2a8LVatULqijD-1-1-0-video-0"
  }
}
ErrorCode:
	(FileNotExistsError, "Failed to create file.")
	(CmdUnsupportedAlgo, "Unsupported algo")
	(StdSystemError, "Failed to remove cache to file")
```



## Get binary

```YAML
Request:
type get_binary_request = {
  "version": "1.0",
  "method": "get_binary",
  "id": 2,
  "params": {
    "access_token": "eyJhbGciOiAiRVMyNTYiLCAia2lkIjogImRpZDplbGFzdG9zOmlxSndzblJCZTZXUlFFYU1USndTYXFNYnZiOTUyWDhWQlMjcHJpbWFyeSJ9.eyJpc3MiOiAiZGlkOmVsYXN0b3M6aXFKd3NuUkJlNldSUUVhTVRKd1NhcU1idmI5NTJYOFZCUyIsICJzdWIiOiAiZGlkOmVsYXN0b3M6aW1aZ0FvOVczOFZ6bzFwSlFmSHA2TkpwOUxac3JuUlBSciIsICJleHAiOiAxNjEwNjc5MDgyLCAidWlkIjogMSwgIm5hbWUiOiAidGVzdCIsICJlbWFpbCI6ICJOQSJ9.dY1N2415mP4A8jTrFaeEGp8dzZHQ4D3FUNWr3Wm75b3x0VxI-TDZGKT72v84VyAGKFJZVYb4CtKbl86DWGMRZw",
    "key": "B4JEjs9vQFmsmXZ8ZiMaZkGGLBctQod2a8LVatULqijD-1-1-0-video-0"
  }
}
Response:
type get_binary_response = {
  "version": "1.0",
  "id": 2,
  "result": {
    "key": "B4JEjs9vQFmsmXZ8ZiMaZkGGLBctQod2a8LVatULqijD-1-1-0-video-0",
    "algo": "None",
    "checksum": "",
    "content": Any
  }
}
ErrorCode:
	(FileNotExistsError, "Failed to create file.")
	(SizeOverflowError, "File size is too large.")
	(OutOfMemoryError, "Failed to allocate memory.")
```