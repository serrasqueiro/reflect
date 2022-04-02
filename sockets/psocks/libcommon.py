# libcommon.py

class OSErrorWrap(OSError):
    def __init__(self, errno=0, strerror=""):
        super().__init__(errno, strerror)

