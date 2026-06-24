-- AlterTable
ALTER TABLE "Device" ADD COLUMN     "scanTotal" INTEGER NOT NULL DEFAULT 0,
ADD COLUMN     "statsRaw" TEXT;

-- AlterTable
ALTER TABLE "Profile" ADD COLUMN     "scanCount" INTEGER NOT NULL DEFAULT 0;
