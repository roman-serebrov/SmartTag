/*
  Warnings:

  - A unique constraint covering the columns `[hwId]` on the table `Device` will be added. If there are existing duplicate values, this will fail.

*/
-- AlterTable
ALTER TABLE "Device" ADD COLUMN     "hwId" TEXT;

-- CreateIndex
CREATE UNIQUE INDEX "Device_hwId_key" ON "Device"("hwId");
