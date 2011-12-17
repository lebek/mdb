from mdb.task import BasicTask


def findAll(string, sub):
    end = len(string)
    start = 0

    while start < end:
        index = string.find(sub, start, end)
        if index == -1:
            break

        yield index
        start = index + 1


def clean(string):
    s = ""
    for c in string:
        if 31 < ord(c) < 127:
            s += c
        else:
            s += '.'

    return s


if __name__ == "__main__":
    from sys import argv

    pid = int(argv[1])
    term = argv[2]

    t = BasicTask(pid)
    t.attach()
    print t.basicInfo()
    for pos, r in enumerate(t.iterRegions()):
        print "Scanning region #%d @ 0x%0.2X of size %d..." % (
            pos, r.address, r.size)

        data = r.read()
        for i in findAll(data, term):
            string = clean(data[(i-50):(i+50)])
            print " 0x%0.2X %s" % (r.address+i, string)
