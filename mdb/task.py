from mdb.core import Task


class BasicTask(Task):

    def iterRegions(self):
        i = 0

        while True:
            region = self.findRegion(i)
            if not region:
                break

            yield region
            i = region.address + region.size
