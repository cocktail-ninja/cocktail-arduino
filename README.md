# Cocktail Ninja - Arduino Yun API

### API Specifications

#### [/status](#)

__Response:__

    STATUS 503
    { "status": "busy" }

    STATUS 404
    { "status": "glass not found" }

    STATUS 200
    { "status": "ready" }

#### [/make_drink](#)

__Request:__ `/{cid}-{amount}`

- `cid`: Component id (Pump: P1, P2, P3, P4, P5, P6 - Valve: V1, V2, V3, V4)
- `amount`: amount number (in ml)

E.g. pouring Valve-1 1000ml, Pump-1 500ml and Pump-2 200ml

    /make_drink/v1-1000/p1-500/p2-200

__Response:__

- `ready_in`: total pouring time

```
{ "ready_in": 1500 }
```

#### [/clean](#)

__Request:__ `/{cleaning_duration}`

- `cleaning_duration`: Cleaning duration (in second, default 60s)

E.g. cleaning the machine for 30 second

    /clean/30

__Response:__

    { "completed_in": 30 }
