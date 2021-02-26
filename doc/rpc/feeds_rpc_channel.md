# Feeds channel rpc

## Get multi comments
```YAML
Request:
type get_multi_comments_request = {
  "version": "1.0",
  "method": "get_multi_comments",
  "id": 600,
  "params": {
    "access_token": "eyJhbGciOiAiRVMyNTYiLCAidHlwIjogIkpXVCIsICJ2ZXJzaW9uIjogIjEuMCIsICJraWQiOiAiZGlkOmVsYXN0b3M6aXFKd3NuUkJlNldSUUVhTVRKd1NhcU1idmI5NTJYOFZCUyNwcmltYXJ5In0.eyJpc3MiOiJkaWQ6ZWxhc3RvczppcUp3c25SQmU2V1JRRWFNVEp3U2FxTWJ2Yjk1Mlg4VkJTIiwiZXhwIjoxNjE0MTM0NjkwLCJhdWQiOiJkaWQ6ZWxhc3RvczppbVpnQW85VzM4VnpvMXBKUWZIcDZOSnA5TFpzcm5SUFJyIiwic3ViIjoiQWNjZXNzVG9rZW4iLCJuYW1lIjoidGVzdCIsInVzZXJEaWQiOiJkaWQ6ZWxhc3RvczppbVpnQW85VzM4VnpvMXBKUWZIcDZOSnA5TFpzcm5SUFJyIiwiZW1haWwiOiJOQSIsImFwcElkIjoiZGlkOmVsYXN0b3M6aXF0V1JWano3Z3NZaHl1UUViMWhZTk5tV1F0MVo5Z2VYZyIsImFwcEluc3RhbmNlRGlkIjoiZGlkOmVsYXN0b3M6aW1aZ0FvOVczOFZ6bzFwSlFmSHA2TkpwOUxac3JuUlBSciIsInVpZCI6MX0.vVk3WZ4jrBvSXssmCK6sVDJataJUMbpwg3jBPyJeRCJG4rnQUlcetXyh1EZwxdUzWAjsTA81-8eecp9CNgJbdQ",
    "channel_id": 0,
    "post_id": 0,
    "by": 2, // 1:id, 2:last_update, 3:created_at
    "upper_bound": 0,
    "lower_bound": 1611632054,
    "max_count": 0
  }
}
Response:
type get_multi_comments_response = {
  "version": "1.0",
  "id": 600,
  "result": {
    "is_last": false,
    "comments": [
      {
        "channel_id": 4,
        "post_id": 46,
        "comment_id": 16,
        "refer_comment_id": 0,
        "status": 1,
        "user_did": "did:elastos:imZgAo9W38Vzo1pJQfHp6NJp9LZsrnRPRr",
        "user_name": "test",
        "content": "?9",
        "likes": 0,
        "created_at": 1608520420,
        "updated_at": 1608520816
      },
      ......
      {
        "channel_id": 3,
        "post_id": 17,
        "comment_id": 10,
        "refer_comment_id": 0,
        "status": 0,
        "user_did": "did:elastos:imZgAo9W38Vzo1pJQfHp6NJp9LZsrnRPRr",
        "user_name": "test",
        "content": "?]rtyuiiioooollhhjkkokioooiffthjjiiiiiiifgjkkkkkkjjkpp",
        "likes": 0,
        "created_at": 1607417077,
        "updated_at": 1607417077
      }
    ]
  }
}
ErrorCode:
	(InvalidArgument, "Invalid argument.")
```



## Get multi likes and comments count

```YAML
Request:
type get_multi_likes_and_comments_count_request = {
  "version": "1.0",
  "method": "get_multi_likes_and_comments_count",
  "id": 33,
  "params": {
    "access_token": "eyJhbGciOiAiRVMyNTYiLCAidHlwIjogIkpXVCIsICJ2ZXJzaW9uIjogIjEuMCIsICJraWQiOiAiZGlkOmVsYXN0b3M6aXFKd3NuUkJlNldSUUVhTVRKd1NhcU1idmI5NTJYOFZCUyNwcmltYXJ5In0.eyJpc3MiOiJkaWQ6ZWxhc3RvczppcUp3c25SQmU2V1JRRWFNVEp3U2FxTWJ2Yjk1Mlg4VkJTIiwiZXhwIjoxNjE0MTM0NjkwLCJhdWQiOiJkaWQ6ZWxhc3RvczppbVpnQW85VzM4VnpvMXBKUWZIcDZOSnA5TFpzcm5SUFJyIiwic3ViIjoiQWNjZXNzVG9rZW4iLCJuYW1lIjoidGVzdCIsInVzZXJEaWQiOiJkaWQ6ZWxhc3RvczppbVpnQW85VzM4VnpvMXBKUWZIcDZOSnA5TFpzcm5SUFJyIiwiZW1haWwiOiJOQSIsImFwcElkIjoiZGlkOmVsYXN0b3M6aXF0V1JWano3Z3NZaHl1UUViMWhZTk5tV1F0MVo5Z2VYZyIsImFwcEluc3RhbmNlRGlkIjoiZGlkOmVsYXN0b3M6aW1aZ0FvOVczOFZ6bzFwSlFmSHA2TkpwOUxac3JuUlBSciIsInVpZCI6MX0.vVk3WZ4jrBvSXssmCK6sVDJataJUMbpwg3jBPyJeRCJG4rnQUlcetXyh1EZwxdUzWAjsTA81-8eecp9CNgJbdQ",
    "channel_id": 0,
    "post_id": 0,
    "by": 2, // 1:id, 2:last_update, 3:created_at
    "upper_bound": 0,
    "lower_bound": 1612057216418,
    "max_count": 0
  }
}
Response:
type get_multi_likes_and_comments_count_response = {
  "version": "1.0",
  "id": 33,
  "result": {
    "is_last": false,
    "posts": [
      {
        "channel_id": 1,
        "post_id": 97,
        "comments_count": 0,
        "likes_count": 0
      },
      ......
      {
        "channel_id": 4,
        "post_id": 46,
        "comments_count": 17,
        "likes_count": 0
      }
    ]
  }
}
ErrorCode:
	(InvalidArgument, "Invalid argument.")
```



## Get multi subscribers count

```YAML
Request:
type get_multi_subscribers_count_request = {
  "version": "1.0",
  "method": "get_multi_subscribers_count",
  "id": 27,
  "params": {
    "access_token": "eyJhbGciOiAiRVMyNTYiLCAidHlwIjogIkpXVCIsICJ2ZXJzaW9uIjogIjEuMCIsICJraWQiOiAiZGlkOmVsYXN0b3M6aXFKd3NuUkJlNldSUUVhTVRKd1NhcU1idmI5NTJYOFZCUyNwcmltYXJ5In0.eyJpc3MiOiJkaWQ6ZWxhc3RvczppcUp3c25SQmU2V1JRRWFNVEp3U2FxTWJ2Yjk1Mlg4VkJTIiwiZXhwIjoxNjE0MTM0NjkwLCJhdWQiOiJkaWQ6ZWxhc3RvczppbVpnQW85VzM4VnpvMXBKUWZIcDZOSnA5TFpzcm5SUFJyIiwic3ViIjoiQWNjZXNzVG9rZW4iLCJuYW1lIjoidGVzdCIsInVzZXJEaWQiOiJkaWQ6ZWxhc3RvczppbVpnQW85VzM4VnpvMXBKUWZIcDZOSnA5TFpzcm5SUFJyIiwiZW1haWwiOiJOQSIsImFwcElkIjoiZGlkOmVsYXN0b3M6aXF0V1JWano3Z3NZaHl1UUViMWhZTk5tV1F0MVo5Z2VYZyIsImFwcEluc3RhbmNlRGlkIjoiZGlkOmVsYXN0b3M6aW1aZ0FvOVczOFZ6bzFwSlFmSHA2TkpwOUxac3JuUlBSciIsInVpZCI6MX0.vVk3WZ4jrBvSXssmCK6sVDJataJUMbpwg3jBPyJeRCJG4rnQUlcetXyh1EZwxdUzWAjsTA81-8eecp9CNgJbdQ",
    "channel_id": 0
  }
}
Response:
type get_multi_subscribers_count_response = {
  "version": "1.0",
  "id": 27,
  "result": {
    "is_last": true,
    "channels": [
      {
        "channel_id": 1,
        "subscribers_count": 1
      },
      ......
      {
        "channel_id": 8,
        "subscribers_count": 0
      }
    ]
  }
}
ErrorCode:
	(InvalidArgument, "Invalid argument.")
```