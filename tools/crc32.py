def crc32str(data):
    crc = 0xffffffff
    for i in range(len(data)):
        crc = crc ^ data[i]
        for j in range(8):
            crc = (crc >> 1) ^ (0xedb88320 & -(crc & 1))
    return 0xffffffff & ~crc


if __name__ == '__main__':

    print('%X' % crc32str('\002\001\000\000'))
