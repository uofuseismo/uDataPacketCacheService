# About

This is an in-memory cache for seismic data packets read from the import streams.  This is designed to keep the last, approximately, 5 minutes of data in memory.  It is useful for near-real-time applications looking to apply some form of inference on a continuous waveform.  For longer duration queries, it is recommended to use the [uWaveServer](https://github.com/uofuseismo/uWaveServer").

# APIs

To define the service

    git subtree add --prefix uDataPacketCacheServiceAPI https://github.com/uofuseismo/uDataPacketCacheServiceAPI.git main --squash

To be able to import data packets

    git subtree add --prefix uDataPacketServiceAPI https://github.com/uofuseismo/uDataPacketServiceAPI.git main --squash

