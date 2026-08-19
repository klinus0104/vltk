-- MySQL dump 10.13  Distrib 5.7.44, for Linux (x86_64)
--
-- Host: localhost    Database: server1
-- ------------------------------------------------------
-- Server version	5.7.44

/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET @OLD_CHARACTER_SET_RESULTS=@@CHARACTER_SET_RESULTS */;
/*!40101 SET @OLD_COLLATION_CONNECTION=@@COLLATION_CONNECTION */;
/*!40101 SET NAMES utf8 */;
/*!40103 SET @OLD_TIME_ZONE=@@TIME_ZONE */;
/*!40103 SET TIME_ZONE='+00:00' */;
/*!40014 SET @OLD_UNIQUE_CHECKS=@@UNIQUE_CHECKS, UNIQUE_CHECKS=0 */;
/*!40014 SET @OLD_FOREIGN_KEY_CHECKS=@@FOREIGN_KEY_CHECKS, FOREIGN_KEY_CHECKS=0 */;
/*!40101 SET @OLD_SQL_MODE=@@SQL_MODE, SQL_MODE='NO_AUTO_VALUE_ON_ZERO' */;
/*!40111 SET @OLD_SQL_NOTES=@@SQL_NOTES, SQL_NOTES=0 */;

--
-- Current Database: `server1`
--

CREATE DATABASE /*!32312 IF NOT EXISTS*/ `server1` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci */;

USE `server1`;

--
-- Table structure for table `OfflineMsg`
--

DROP TABLE IF EXISTS `OfflineMsg`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `OfflineMsg` (
  `ID` int(11) NOT NULL AUTO_INCREMENT,
  `Receiver` tinyblob,
  `Sender` tinyblob,
  `Msg` longblob,
  `LastModify` datetime DEFAULT NULL,
  UNIQUE KEY `ID` (`ID`),
  KEY `Receiver` (`Receiver`(32)),
  KEY `LastModify` (`LastModify`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `OfflineMsg`
--

LOCK TABLES `OfflineMsg` WRITE;
/*!40000 ALTER TABLE `OfflineMsg` DISABLE KEYS */;
/*!40000 ALTER TABLE `OfflineMsg` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `PlayerTag`
--

DROP TABLE IF EXISTS `PlayerTag`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `PlayerTag` (
  `rolename` varchar(32) COLLATE utf8mb4_unicode_ci NOT NULL,
  `data` blob,
  PRIMARY KEY (`rolename`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `PlayerTag`
--

LOCK TABLES `PlayerTag` WRITE;
/*!40000 ALTER TABLE `PlayerTag` DISABLE KEYS */;
/*!40000 ALTER TABLE `PlayerTag` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `Relation`
--

DROP TABLE IF EXISTS `Relation`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `Relation` (
  `ID` int(11) NOT NULL AUTO_INCREMENT,
  `RoleName` tinyblob NOT NULL,
  `Data` longblob,
  PRIMARY KEY (`RoleName`(32)),
  UNIQUE KEY `ID` (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `Relation`
--

LOCK TABLES `Relation` WRITE;
/*!40000 ALTER TABLE `Relation` DISABLE KEYS */;
/*!40000 ALTER TABLE `Relation` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `RoleBack`
--

DROP TABLE IF EXISTS `RoleBack`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `RoleBack` (
  `ID` int(11) NOT NULL AUTO_INCREMENT,
  `Account` varchar(32) COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `RoleName` tinyblob,
  `RoleData` longblob,
  `LastModify` datetime DEFAULT NULL,
  `Reason` tinyint(4) DEFAULT NULL,
  UNIQUE KEY `ID` (`ID`),
  KEY `Account` (`Account`),
  KEY `RoleName` (`RoleName`(32))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `RoleBack`
--

LOCK TABLES `RoleBack` WRITE;
/*!40000 ALTER TABLE `RoleBack` DISABLE KEYS */;
/*!40000 ALTER TABLE `RoleBack` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `ShareData`
--

DROP TABLE IF EXISTS `ShareData`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `ShareData` (
  `ID` int(11) NOT NULL AUTO_INCREMENT,
  `ShareKey` tinyblob NOT NULL,
  `Param1` int(10) unsigned NOT NULL,
  `Param2` int(10) unsigned NOT NULL,
  `Data` longblob,
  UNIQUE KEY `ID` (`ID`),
  UNIQUE KEY `ShareKey` (`ShareKey`(32),`Param1`,`Param2`)
) ENGINE=InnoDB AUTO_INCREMENT=192 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `ShareData`
--

LOCK TABLES `ShareData` WRITE;
/*!40000 ALTER TABLE `ShareData` DISABLE KEYS */;
INSERT INTO `ShareData` VALUES (1,_binary 'GAME_DUNGEON\0',0,0,''),(3,_binary 'FUNC_SERVER_OPEN_TIME\0',0,0,_binary '\0\0\0dŠ`\ÚA'),(4,_binary '³\ÇÕ½-³\Ç\Çø\Êý¾\Ý\0',0,0,_binary '\0\0\0'),(5,_binary '³\ÇÕ½-³\Ç\Çø\Êý¾\Ý\0',1,0,_binary '\0\0\0\0'),(6,_binary '³\ÇÕ½-³\Ç\Çø\Êý¾\Ý\0',1,1,_binary '\0'),(7,_binary '³\ÇÕ½-³\Ç\Çø\Êý¾\Ý\0',1,2,_binary '\0'),(8,_binary '³\ÇÕ½-³\Ç\Çø\Êý¾\Ý\0',1,3,_binary '\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0'),(9,_binary '³\ÇÕ½-³\Ç\Çø\Êý¾\Ý\0',1,4,_binary '\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0'),(10,_binary '³\ÇÕ½-³\Ç\Çø\Êý¾\Ý\0',1,5,_binary '\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0'),(11,_binary '³\ÇÕ½-³\Ç\Çø\Êý¾\Ý\0',1,6,_binary 'ÿÿÿÿ'),(12,_binary '³\ÇÕ½-³\Ç\Çø\Êý¾\Ý\0',1,7,_binary 'ÿÿÿÿ'),(13,_binary '³\ÇÕ½-³\Ç\Çø\Êý¾\Ý\0',1,8,_binary '\0\0\0\0'),(14,_binary '³\ÇÕ½-³\Ç\Çø\Êý¾\Ý\0',2,0,_binary '\0\0\0'),(15,_binary '³\ÇÕ½-³\Ç\Çø\Êý¾\Ý\0',2,1,_binary '\0'),(16,_binary '³\ÇÕ½-³\Ç\Çø\Êý¾\Ý\0',2,2,_binary '\0'),(17,_binary '³\ÇÕ½-³\Ç\Çø\Êý¾\Ý\0',2,3,_binary '\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0'),(18,_binary '³\ÇÕ½-³\Ç\Çø\Êý¾\Ý\0',2,4,_binary '\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0'),(19,_binary '³\ÇÕ½-³\Ç\Çø\Êý¾\Ý\0',2,5,_binary '\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0'),(20,_binary '³\ÇÕ½-³\Ç\Çø\Êý¾\Ý\0',2,6,_binary 'ÿÿÿÿ'),(21,_binary '³\ÇÕ½-³\Ç\Çø\Êý¾\Ý\0',2,7,_binary 'ÿÿÿÿ'),(22,_binary '³\ÇÕ½-³\Ç\Çø\Êý¾\Ý\0',2,8,_binary '\0\0\0\0'),(23,_binary '³\ÇÕ½-³\Ç\Çø\Êý¾\Ý\0',3,0,_binary '\0\0\0'),(24,_binary '³\ÇÕ½-³\Ç\Çø\Êý¾\Ý\0',3,1,_binary '\0'),(25,_binary '³\ÇÕ½-³\Ç\Çø\Êý¾\Ý\0',3,2,_binary '\0'),(26,_binary '³\ÇÕ½-³\Ç\Çø\Êý¾\Ý\0',3,3,_binary '\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0'),(27,_binary '³\ÇÕ½-³\Ç\Çø\Êý¾\Ý\0',3,4,_binary '\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0'),(28,_binary '³\ÇÕ½-³\Ç\Çø\Êý¾\Ý\0',3,5,_binary '\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0'),(29,_binary '³\ÇÕ½-³\Ç\Çø\Êý¾\Ý\0',3,6,_binary 'ÿÿÿÿ'),(30,_binary '³\ÇÕ½-³\Ç\Çø\Êý¾\Ý\0',3,7,_binary 'ÿÿÿÿ'),(31,_binary '³\ÇÕ½-³\Ç\Çø\Êý¾\Ý\0',3,8,_binary '\0\0\0\0'),(32,_binary '³\ÇÕ½-³\Ç\Çø\Êý¾\Ý\0',4,0,_binary '\0\0\0'),(33,_binary '³\ÇÕ½-³\Ç\Çø\Êý¾\Ý\0',4,1,_binary '\0'),(34,_binary '³\ÇÕ½-³\Ç\Çø\Êý¾\Ý\0',4,2,_binary '\0'),(35,_binary '³\ÇÕ½-³\Ç\Çø\Êý¾\Ý\0',4,3,_binary '\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0'),(36,_binary '³\ÇÕ½-³\Ç\Çø\Êý¾\Ý\0',4,4,_binary '\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0'),(37,_binary '³\ÇÕ½-³\Ç\Çø\Êý¾\Ý\0',4,5,_binary '\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0'),(38,_binary '³\ÇÕ½-³\Ç\Çø\Êý¾\Ý\0',4,6,_binary 'ÿÿÿÿ'),(39,_binary '³\ÇÕ½-³\Ç\Çø\Êý¾\Ý\0',4,7,_binary 'ÿÿÿÿ'),(40,_binary '³\ÇÕ½-³\Ç\Çø\Êý¾\Ý\0',4,8,_binary '\0\0\0\0'),(41,_binary '³\ÇÕ½-³\Ç\Çø\Êý¾\Ý\0',5,0,_binary '\0\0\0'),(42,_binary '³\ÇÕ½-³\Ç\Çø\Êý¾\Ý\0',5,1,_binary '\0'),(43,_binary '³\ÇÕ½-³\Ç\Çø\Êý¾\Ý\0',5,2,_binary '\0'),(44,_binary '³\ÇÕ½-³\Ç\Çø\Êý¾\Ý\0',5,3,_binary '\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0'),(45,_binary '³\ÇÕ½-³\Ç\Çø\Êý¾\Ý\0',5,4,_binary '\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0'),(46,_binary '³\ÇÕ½-³\Ç\Çø\Êý¾\Ý\0',5,5,_binary '\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0'),(47,_binary '³\ÇÕ½-³\Ç\Çø\Êý¾\Ý\0',5,6,_binary 'ÿÿÿÿ'),(48,_binary '³\ÇÕ½-³\Ç\Çø\Êý¾\Ý\0',5,7,_binary 'ÿÿÿÿ'),(49,_binary '³\ÇÕ½-³\Ç\Çø\Êý¾\Ý\0',5,8,_binary '\0\0\0\0'),(50,_binary '³\ÇÕ½-³\Ç\Çø\Êý¾\Ý\0',6,0,_binary '\0\0\0'),(51,_binary '³\ÇÕ½-³\Ç\Çø\Êý¾\Ý\0',6,1,_binary '\0'),(52,_binary '³\ÇÕ½-³\Ç\Çø\Êý¾\Ý\0',6,2,_binary '\0'),(53,_binary '³\ÇÕ½-³\Ç\Çø\Êý¾\Ý\0',6,3,_binary '\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0'),(54,_binary '³\ÇÕ½-³\Ç\Çø\Êý¾\Ý\0',6,4,_binary '\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0'),(55,_binary '³\ÇÕ½-³\Ç\Çø\Êý¾\Ý\0',6,5,_binary '\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0'),(56,_binary '³\ÇÕ½-³\Ç\Çø\Êý¾\Ý\0',6,6,_binary 'ÿÿÿÿ'),(57,_binary '³\ÇÕ½-³\Ç\Çø\Êý¾\Ý\0',6,7,_binary 'ÿÿÿÿ'),(58,_binary '³\ÇÕ½-³\Ç\Çø\Êý¾\Ý\0',6,8,_binary '\0\0\0\0'),(59,_binary '³\ÇÕ½-³\Ç\Çø\Êý¾\Ý\0',7,0,_binary '\0\0\0'),(60,_binary '³\ÇÕ½-³\Ç\Çø\Êý¾\Ý\0',7,1,_binary '\0'),(61,_binary '³\ÇÕ½-³\Ç\Çø\Êý¾\Ý\0',7,2,_binary '\0'),(62,_binary '³\ÇÕ½-³\Ç\Çø\Êý¾\Ý\0',7,3,_binary '\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0'),(63,_binary '³\ÇÕ½-³\Ç\Çø\Êý¾\Ý\0',7,4,_binary '\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0'),(64,_binary '³\ÇÕ½-³\Ç\Çø\Êý¾\Ý\0',7,5,_binary '\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0'),(65,_binary '³\ÇÕ½-³\Ç\Çø\Êý¾\Ý\0',7,6,_binary 'ÿÿÿÿ'),(66,_binary '³\ÇÕ½-³\Ç\Çø\Êý¾\Ý\0',7,7,_binary 'ÿÿÿÿ'),(67,_binary '³\ÇÕ½-³\Ç\Çø\Êý¾\Ý\0',7,8,_binary '\0\0\0\0'),(68,_binary '³\ÇÕ½-³\Ç\Çø\Êý¾\Ý\0',8,0,_binary '\0\0\0'),(69,_binary '³\ÇÕ½-³\Ç\Çø\Êý¾\Ý\0',8,1,_binary '\0'),(70,_binary '³\ÇÕ½-³\Ç\Çø\Êý¾\Ý\0',8,2,_binary '\0'),(71,_binary '³\ÇÕ½-³\Ç\Çø\Êý¾\Ý\0',8,3,_binary '\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0'),(72,_binary '³\ÇÕ½-³\Ç\Çø\Êý¾\Ý\0',8,4,_binary '\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0'),(73,_binary '³\ÇÕ½-³\Ç\Çø\Êý¾\Ý\0',8,5,_binary '\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0'),(74,_binary '³\ÇÕ½-³\Ç\Çø\Êý¾\Ý\0',8,6,_binary 'ÿÿÿÿ'),(75,_binary '³\ÇÕ½-³\Ç\Çø\Êý¾\Ý\0',8,7,_binary 'ÿÿÿÿ'),(76,_binary '³\ÇÕ½-³\Ç\Çø\Êý¾\Ý\0',8,8,_binary '\0\0\0\0'),(77,_binary 'DynMapInfo\0',1,0,_binary '\ç\0'),(78,_binary 'GAME_LADDER\0',10277,1,_binary '\0\0\0B\åÙ§\ÒÙ§¹t\ÙMa		150 c\Êp | TS:\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0'),(79,_binary 'GAME_LADDER\0',10281,1,_binary '\0\0\01 LonNGaMi\0<\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0'),(80,_binary 'GAME_LADDER\0',10287,1,_binary '\0\0\01 ThieuLam\0\È\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0'),(81,_binary 'GAME_LEAGUE\0',4,3989547523,_binary 'ˆ\0\0\0\0\0\05\0¸n Hoa §¨ng\0 Chi\Õn\0 PHAM\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0hƒi\06\0\0\0\0\0\0\0\0\0\0\0\0\0\05\0¸n Hoa §¨ng\0 Chi\Õn\0 PHAM\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\'hƒi\0\0\0\0\0\0\0'),(82,_binary 'GAME_LEAGUE\0',4,3989547538,_binary 'ˆ\0\0\0\0\0\06\0¸n Hoa §¨ng\0 Chi\Õn\0 PHAM\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0hƒi\06\0\0\0\0\0\0\0\0\0\0\0\0\0\06\0¸n Hoa §¨ng\0 Chi\Õn\0 PHAM\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\'hƒi\0\0\0\0\0\0\0'),(83,_binary 'GAME_LEAGUE\0',4,3989547553,_binary 'ˆ\0\0\0\0\0\07\0¸n Hoa §¨ng\0 Chi\Õn\0 PHAM\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0hƒi\06\0\0\0\0\0\0\0\0\0\0\0\0\0\07\0¸n Hoa §¨ng\0 Chi\Õn\0 PHAM\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\'hƒi\0\0\0\0\0\0\0'),(84,_binary 'GAME_LEAGUE\0',4,3989547719,_binary 'ˆ\0\0\0\0\0\01\0¸n Hoa §¨ng\0 Chi\Õn\0 PHAM\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0hƒi\06\0\0\0\0\0\0\0\0\0\0\0\0\0\01\0¸n Hoa §¨ng\0 Chi\Õn\0 PHAM\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\'hƒi\0\0\0\0\0\0\0'),(85,_binary 'GAME_LEAGUE\0',4,3989547734,_binary 'ˆ\0\0\0\0\0\02\0¸n Hoa §¨ng\0 Chi\Õn\0 PHAM\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0hƒi\06\0\0\0\0\0\0\0\0\0\0\0\0\0\02\0¸n Hoa §¨ng\0 Chi\Õn\0 PHAM\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\'hƒi\0\0\0\0\0\0\0'),(86,_binary 'GAME_LEAGUE\0',4,3989547749,_binary 'ˆ\0\0\0\0\0\03\0¸n Hoa §¨ng\0 Chi\Õn\0 PHAM\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0hƒi\06\0\0\0\0\0\0\0\0\0\0\0\0\0\03\0¸n Hoa §¨ng\0 Chi\Õn\0 PHAM\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\'hƒi\0\0\0\0\0\0\0'),(87,_binary 'GAME_LEAGUE\0',4,3989547764,_binary 'ˆ\0\0\0\0\0\04\0¸n Hoa §¨ng\0 Chi\Õn\0 PHAM\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0hƒi\06\0\0\0\0\0\0\0\0\0\0\0\0\0\04\0¸n Hoa §¨ng\0 Chi\Õn\0 PHAM\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\'hƒi\0\0\0\0\0\0\0'),(88,_binary 'GAME_LEAGUE\0',500,117889592,_binary 'ˆ\0\0\0ô\0\0TONG_SPFESTIVAL\0P VAT PHAM\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\00\0\0\0\0\0\0\0\0\0\0\0\0\0\0TONG_SPFESTIVAL\0P VAT PHAM\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0'),(89,_binary 'GAME_LEAGUE\0',500,706678739,_binary 'ˆ\0\0\0ô\0\0BANG HOI THU THAP VAT PHAM\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\00\0\0\0\0\0\0\0\0\0\0\0\0\0\0BANG HOI THU THAP VAT PHAM\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0'),(90,_binary 'GAME_LEAGUE\0',500,1691131935,_binary 'ˆ\0\0\0ô\0\0TONG_SHREWMOUSE\0P VAT PHAM\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\00\0\0\0\0\0\0\0\0\0\0\0\0\0\0TONG_SHREWMOUSE\0P VAT PHAM\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0'),(91,_binary 'GAME_LEAGUE\0',500,2454813626,_binary '”\0\0\0ô\0\0WLLS\0N¨m ®­\îc m\ïa l­u kh¸ch ®ñ kª ®\ån 	Y quan ®¬\0\0\0\0\00\0\Z\0\0\0\0\0\0\0\0\0\0\0\0\0WLLS\0­\îng kh«ng lóc nµo d¹ g\â cö\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0Ÿ\0\0\0\0\0\0\0\0\0\0\0\0'),(92,_binary 'GAME_LEAGUE\0',502,686111018,_binary 'ˆ\0\0\0ö\0\0T\èng Kim\0h §¹i Chi\Õn\0 PHAM\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\00\0\0\0\0\0\0\0\0\0\0\0\0\0\0T\èng Kim\0h §¹i Chi\Õn\0 PHAM\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0'),(93,_binary 'GAME_LEAGUE\0',502,809157886,_binary 'ˆ\0\0\0ö\0\0§o¸n Hoa §¨ng\0 Chi\Õn\0 PHAM\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\00\0\0\0\0\0\0\0\0\0\0\0\0\0\0§o¸n Hoa §¨ng\0 Chi\Õn\0 PHAM\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0'),(94,_binary 'GAME_LEAGUE\0',502,1133751788,_binary 'ˆ\0\0\0ö\0\0V­\ît ¶i\0\0h §¹i Chi\Õn\0 PHAM\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\00\0\0\0\0\0\0\0\0\0\0\0\0\0\0V­\ît ¶i\0\0h §¹i Chi\Õn\0 PHAM\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0'),(95,_binary 'GAME_LEAGUE\0',502,1416635687,_binary 'ˆ\0\0\0ö\0\0Liªn §\Êu\0Hoµng Kim\0\0T PHAM\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\00\0\0\0\0\0\0\0\0\0\0\0\0\0\0Liªn §\Êu\0Hoµng Kim\0\0T PHAM\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0'),(96,_binary 'GAME_LEAGUE\0',502,1899390498,_binary 'ˆ\0\0\0ö\0\0Boss §¹i Hoµng Kim\0\0T PHAM\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\00\0\0\0\0\0\0\0\0\0\0\0\0\0\0Boss §¹i Hoµng Kim\0\0T PHAM\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0'),(97,_binary 'GAME_LEAGUE\0',502,2711021296,_binary 'ˆ\0\0\0ö\0\0Th\Êt Thµnh §¹i Chi\Õn\0 PHAM\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\00\0\0\0\0\0\0\0\0\0\0\0\0\0\0Th\Êt Thµnh §¹i Chi\Õn\0 PHAM\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0'),(98,_binary 'GAME_LEAGUE\0',502,3275668761,_binary 'ˆ\0\0\0ö\0\0Phong L¨ng §\é\0 Kim\0\0T PHAM\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\00\0\0\0\0\0\0\0\0\0\0\0\0\0\0Phong L¨ng §\é\0 Kim\0\0T PHAM\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0'),(99,_binary 'GAME_LEAGUE\0',502,3770955735,_binary 'ˆ\0\0\0ö\0\0Boss Ti\Óu Hoµng Kim\0T PHAM\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\00\0\0\0\0\0\0\0\0\0\0\0\0\0\0Boss Ti\Óu Hoµng Kim\0T PHAM\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0'),(100,_binary 'GoldBoss\0',511,22,_binary 'ÿ\0\0Z\0\0\0\0\0\0'),(101,_binary 'GoldBoss\0',513,94,_binary '\0\0Z\0\0\0\0\0\0'),(102,_binary 'GoldBoss\0',523,180,_binary '\0\0Z\0\0\0\0\0\0'),(103,_binary 'KEY_EMPIRE\0',0,0,_binary '\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0'),(104,_binary 'KEY_EMPIRE\0',0,1,_binary '\0\0\0\0'),(105,_binary 'LotteryLog\0',0,0,_binary '\0\0\0\0\0\0\0\0\0\0\0\0'),(106,_binary 'GAME_LEAGUE\0',502,688385051,_binary 'ˆ\0\0\0ö\0\0Phuc Duyen - Phao Bong\0\0. 	4	0	Ba ng­\êi ®i 	Hai \0\0\0\0\00\0\0\0\0\0\0\0\0\0\0\0\0\0\0Phuc Duyen - Phao Bong\0\0		1	50	1\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0'),(107,_binary 'GAME_LEAGUE\0',502,1081242479,_binary 'ˆ\0\0\0ö\0\0chunjie2009_dangboss\0oi\0. 	4	0	Ba ng­\êi ®i 	Hai \0\0\0\0\00\0\0\0\0\0\0\0\0\0\0\0\0\0\0chunjie2009_dangboss\0oi\0		1	50	1\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0'),(108,_binary 'GAME_LEAGUE\0',502,1754936316,_binary 'ˆ\0\0\0ö\0\0YANDIBAOZANG_TALK\0m Moi\0. 	4	0	Ba ng­\êi ®i 	Hai \0\0\0\0\00\0\0\0\0\0\0\0\0\0\0\0\0\0\0YANDIBAOZANG_TALK\0m Moi\0		1	50	1\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0'),(109,_binary 'GAME_LEAGUE\0',502,1818746216,_binary 'ˆ\0\0\0ö\0\0Truyen Cong\0ung Nam Moi\0. 	4	0	Ba ng­\êi ®i 	Hai \0\0\0\0\00\0\0\0\0\0\0\0\0\0\0\0\0\0\0Truyen Cong\0ung Nam Moi\0		1	50	1\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0'),(110,_binary 'GAME_LEAGUE\0',502,2504949757,_binary 'ˆ\0\0\0ö\0\0Le vat tinh nhan\0n 2006\0. 	4	0	Ba ng­\êi ®i 	Hai \0\0\0\0\00\0\0\0\0\0\0\0\0\0\0\0\0\0\0Le vat tinh nhan\0n 2006\0		1	50	1\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0'),(111,_binary 'GAME_LEAGUE\0',502,2533843065,_binary 'ˆ\0\0\0ö\0\0SWITH_DAIYITOUSHI\0Bong\0\0. 	4	0	Ba ng­\êi ®i 	Hai \0\0\0\0\00\0\0\0\0\0\0\0\0\0\0\0\0\0\0SWITH_DAIYITOUSHI\0Bong\0\0		1	50	1\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0'),(112,_binary 'GAME_LEAGUE\0',502,3250451853,_binary 'ˆ\0\0\0ö\0\0Hoat dong mua xuan 2006\0. 	4	0	Ba ng­\êi ®i 	Hai \0\0\0\0\00\0\0\0\0\0\0\0\0\0\0\0\0\0\0Hoat dong mua xuan 2006\0		1	50	1\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0'),(113,_binary 'GAME_LEAGUE\0',502,3354647675,_binary 'ˆ\0\0\0ö\0\0Thiep Chuc Mung Nam Moi\0. 	4	0	Ba ng­\êi ®i 	Hai \0\0\0\0\00\0\0\0\0\0\0\0\0\0\0\0\0\0\0Thiep Chuc Mung Nam Moi\0		1	50	1\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0'),(114,_binary 'GAME_LEAGUE\0',502,3850167185,_binary 'ˆ\0\0\0ö\0\0OpenShop\0nh nhan\0n 2006\0. 	4	0	Ba ng­\êi ®i 	Hai \0\0\0\0\00\0\0\0\0\0\0\0\0\0\0\0\0\0\0OpenShop\0nh nhan\0n 2006\0		1	50	1\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0'),(115,_binary 'GAME_LEAGUE\0',502,4028493098,_binary 'ˆ\0\0\0ö\0\0YANDIBAOZANG\0ng Nam Moi\0. 	4	0	Ba ng­\êi ®i 	Hai \0\0\0\0\00\0\0\0\0\0\0\0\0\0\0\0\0\0\0YANDIBAOZANG\0ng Nam Moi\0		1	50	1\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0'),(116,_binary 'GAME_LEAGUE\0',502,4187781415,_binary 'ˆ\0\0\0ö\0\0Do Pho Hoang Kim\0 s­ yªn. 	4	0	Ba ng­\êi ®i 	Hai \0\0\0\0\00\0\0\0\0\0\0\0\0\0\0\0\0\0\0Do Pho Hoang Kim\0									1	50	1\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0'),(120,_binary 'GAME_LEAGUE\0',10000,170495098,_binary 'R\0\0\0\'\0\0stat_goodssale\0sinh bao nhiªu 	¤ th­\íc bay v\Ò ph%¯Tj\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0'),(121,_binary 'BATTLE_1_1\0',1,1,_binary '\0\0\0	\0\0\0 \0\0\0C«ng thµnh chi\Õn (khu vùc Cöu K\0\0\0\0\0\0\0'),(122,_binary 'BATTLE_1_3\0',1,1,_binary '\0\0\0	\0\0\0 \0\0\0C«ng thµnh chi\Õn (khu vùc Cöu K\0\0\0\0\0\0\0'),(123,_binary 'BATTLE_LATEST_INFO\0',1,1,_binary '\0\0\0\0\0\0'),(124,_binary 'BATTLE_LATEST_INFO\0',1,3,_binary '\0\0\0\0\0\0'),(125,_binary 'BATTLE_1_1\0',1,2,_binary '\0\0\0	\0\0\0 \0\0\0C«ng thµnh chi\Õn (khu vùc Cöu K\0\0\0\0\0\0\0\0'),(129,_binary 'BATTLE_2_3\0',1,1,_binary '\0\0\0\0\0\0\0\0\0Qu\èc chi\Õn Hoµng Sa L©m\0\n\0\0\0\0\0\0\0'),(130,_binary 'BATTLE_LATEST_INFO\0',2,3,_binary '\0\0\0\0\0\0'),(131,_binary 'GAME_LEAGUE\0',500,1592025496,_binary 'ˆ\0\0\0ô\0\0HOAT DONG HOA DANG\0 	N¨m ®­\îc m\ïa l­u kh¸ch ®ñ k\0\0\0\0\00\0\0\0\0\0\0\0\0\0\0\0\0\0\0HOAT DONG HOA DANG\0tr­\îng kh«ng \0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0'),(132,_binary 'GAME_LEAGUE\0',500,2999538810,_binary 'Ž\0\0\0ô\0\0QUA HUY HOANG\0DANG\0 	N¨m ®­\îc m\ïa l­u kh¸ch ®ñ k\0\0\0\0\00\0\0\0\0\0\0\0\0\0\0\0\0\0\0QUA HUY HOANG\0DANG\0tr­\îng kh«ng \0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0'),(133,_binary 'BATTLE_1_1\0',1,3,_binary '\0\0\0	\0\0\0 \0\0\0C«ng thµnh chi\Õn (khu vùc Cöu K\0\0\0\0\0\0\0\0'),(134,_binary 'BATTLE_1_1\0',1,4,_binary '\0\0\0\0\0\0\0\0\0Hoµng Sa L©m Ngao Chi\Õn\0\0\0\0\0\0\0\0'),(135,_binary 'BATTLE_1_1\0',1,5,_binary '\0\0\0\n\0\0\0\0\0\0Hoµng Sa L©m Ngao Chi\Õn\0\0\0\0\0\0\0\0'),(139,_binary 'BATTLE_1_1\0',1,6,_binary '\0\0\0\0\0\0 \0\0\0C«ng thµnh chi\Õn (khu vùc Cöu K\0\0\0\0\0\0\0\0'),(143,_binary 'BATTLE_1_1\0',1,7,_binary '\0\0\0\0\0\0\0\0\0Hoµng Sa L©m Ngao Chi\Õn\0\0\0\0\0\0\0\0'),(144,_binary 'BATTLE_1_1\0',1,8,_binary '\0\0\0\0\0\0\0\0\0Hoµng Sa L©m Ngao Chi\Õn\0\0\0\0\0\0\0'),(145,_binary 'BATTLE_1_1\0',1,9,_binary '\0\0\0\0\0\0 \0\0\0C«ng thµnh chi\Õn (khu vùc Cöu K\0\0\0\0\0\0\0'),(146,_binary 'BATTLE_1_3\0',1,2,_binary '\0\0\0\n\0\0\0\0\0\0Hoµng Sa L©m Ngao Chi\Õn\0\0\0\0\0\0\0'),(147,_binary 'BATTLE_1_1\0',1,10,_binary '\0\0\0	\0\0\0 \0\0\0C«ng thµnh chi\Õn (khu vùc Cöu K\0\0\0\0\0\0\0'),(148,_binary 'GAME_LEAGUE\0',500,4044829216,_binary 'Ž\0\0\0ô\0\0h¹t Huy Hoµng\0DANG\0 	N¨m ®­\îc m\ïa l­u kh¸ch ®ñ k\Ö2Vj\06\0\0\0\0\0\0\0\0\0\0\0\0\0\0h¹t Huy Hoµng\0DANG\0tr­\îng kh«ng \0\0\0\0\0\0\0\0\0\'\Ö2Vj\0\0\0\0\0\0\0\0\0\0'),(149,_binary 'BATTLE_1_1\0',1,11,_binary '\0\0\0\n\0\0\0\0\0\0Hoµng Sa L©m Ngao Chi\Õn\0\0\0\0\0\0\0\0'),(150,_binary 'BATTLE_1_1\0',1,12,_binary '\0\0\0\n\0\0\0\0\0\0Hoµng Sa L©m Ngao Chi\Õn\0\0\0\0\0\0\0'),(151,_binary 'BATTLE_1_1\0',1,13,_binary '\n\0\0\0\0\0\0\0\0\0Long V­¬ng Ch©u Chi\Õn\0\0\0\0\0\0\0\0'),(152,_binary 'GAME_LADDER\0',10278,1,_binary '\0\0\01 ThieuLam\0\È\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0'),(153,_binary 'GAME_LADDER\0',10287,2,_binary '\0\0\02 LonNGaMi\0<\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0'),(155,_binary 'BATTLE_1_1\0',1,14,_binary '\0\0\0\n\0\0\0\0\0\0Hoµng Sa L©m Ngao Chi\Õn\0\0\0\0\0\0\0'),(159,_binary 'BATTLE_1_1\0',1,15,_binary '\0\0\0\0\0\0 \0\0\0C«ng thµnh chi\Õn (khu vùc Cöu K\0\0\0\0\0\0\0'),(160,_binary 'BATTLE_1_1\0',1,16,_binary '\0\0\0\n\0\0\0\0\0\0Hoµng Sa L©m Ngao Chi\Õn\0\0\0\0\0\0\0'),(161,_binary 'BATTLE_1_1\0',1,17,_binary '\0\0\0\0\0\0\0\0\0Hoµng Sa L©m Ngao Chi\Õn\0\0\0\0\0\0\0'),(162,_binary 'BATTLE_1_3\0',1,3,_binary '\0\0\0\0\0\0\0\0\0Hoµng Sa L©m Ngao Chi\Õn\0\0\0\0\0\0\0'),(163,_binary 'BATTLE_1_1\0',1,18,_binary '\0\0\0\0\0\0\0\0\0B¹ch M«n Nguyªn Chi\Õn\0\0\0\0\0\0\0'),(167,_binary 'BATTLE_1_1\0',1,19,_binary '\0\0\0	\0\0\0 \0\0\0C«ng thµnh chi\Õn (khu vùc Cöu K\0\0\0\0\0\0\0'),(168,_binary 'BATTLE_1_1\0',1,20,_binary '\0\0\0\n\0\0\0\0\0\0Hoµng Sa L©m Ngao Chi\Õn\0\0\0\0\0\0\0'),(173,_binary 'GAME_LADDER\0',10287,3,_binary '\r\0\0\03 ThienVuong\0<\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0'),(175,_binary 'BATTLE_1_1\0',1,21,_binary '\0\0\0\0\0\0\0\0\0Hoµng Sa L©m Ngao Chi\Õn\0\0\0\0\0\0\0'),(179,_binary 'BATTLE_1_1\0',1,22,_binary '\0\0\0\0\0\0\0\0\0Hoµng Sa L©m Ngao Chi\Õn\0\0\0\0\0\0\0'),(180,_binary 'BATTLE_1_1\0',1,23,_binary '\0\0\0\0\0\0 \0\0\0C«ng thµnh chi\Õn (khu vùc Cöu K\0\0\0\0\0\0\0'),(181,_binary 'BATTLE_1_1\0',1,24,_binary '\0\0\0\0\0\0\0\0\0Hoµng Sa L©m Ngao Chi\Õn\0\0\0\0\0\0\0'),(182,_binary 'BATTLE_1_3\0',1,4,_binary '\0\0\0\0\0\0\0\0\0B¹ch M«n Nguyªn Chi\Õn\0\0\0\0\0\0\0'),(183,_binary 'BATTLE_1_1\0',1,25,_binary '\0\0\0\0\0\0\0\0\0Hoµng Sa L©m Ngao Chi\Õn\0\0\0\0\0\0\0'),(191,_binary 'EVENT_April_zhushuai\0',1,0,_binary '\0\0\0\0\0\0\0\0\0ð?\0\0\0v˜\ÚA');
/*!40000 ALTER TABLE `ShareData` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `Tong`
--

DROP TABLE IF EXISTS `Tong`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `Tong` (
  `ID` int(11) NOT NULL AUTO_INCREMENT,
  `TongID` int(10) unsigned NOT NULL,
  `Data` longblob,
  PRIMARY KEY (`TongID`),
  UNIQUE KEY `ID` (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `Tong`
--

LOCK TABLES `Tong` WRITE;
/*!40000 ALTER TABLE `Tong` DISABLE KEYS */;
/*!40000 ALTER TABLE `Tong` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `TongMember`
--

DROP TABLE IF EXISTS `TongMember`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `TongMember` (
  `ID` int(11) NOT NULL AUTO_INCREMENT,
  `TongID` int(10) unsigned NOT NULL,
  `MemberID` int(10) unsigned NOT NULL,
  `InRecycle` tinyint(3) unsigned NOT NULL,
  `Data` longblob,
  UNIQUE KEY `ID` (`ID`),
  KEY `TongID` (`TongID`,`MemberID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `TongMember`
--

LOCK TABLES `TongMember` WRITE;
/*!40000 ALTER TABLE `TongMember` DISABLE KEYS */;
/*!40000 ALTER TABLE `TongMember` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `TongUnion`
--

DROP TABLE IF EXISTS `TongUnion`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `TongUnion` (
  `ID` int(11) NOT NULL AUTO_INCREMENT,
  `UnionID` int(10) unsigned NOT NULL,
  `Data` longblob,
  PRIMARY KEY (`UnionID`),
  UNIQUE KEY `ID` (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `TongUnion`
--

LOCK TABLES `TongUnion` WRITE;
/*!40000 ALTER TABLE `TongUnion` DISABLE KEYS */;
/*!40000 ALTER TABLE `TongUnion` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `TongZhaoMu`
--

DROP TABLE IF EXISTS `TongZhaoMu`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `TongZhaoMu` (
  `TongID` int(10) unsigned NOT NULL,
  `Data` blob,
  PRIMARY KEY (`TongID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `TongZhaoMu`
--

LOCK TABLES `TongZhaoMu` WRITE;
/*!40000 ALTER TABLE `TongZhaoMu` DISABLE KEYS */;
/*!40000 ALTER TABLE `TongZhaoMu` ENABLE KEYS */;
UNLOCK TABLES;
/*!40103 SET TIME_ZONE=@OLD_TIME_ZONE */;

/*!40101 SET SQL_MODE=@OLD_SQL_MODE */;
/*!40014 SET FOREIGN_KEY_CHECKS=@OLD_FOREIGN_KEY_CHECKS */;
/*!40014 SET UNIQUE_CHECKS=@OLD_UNIQUE_CHECKS */;
/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
/*!40111 SET SQL_NOTES=@OLD_SQL_NOTES */;

-- Dump completed on 2026-07-17  1:58:46
-- MySQL dump 10.13  Distrib 5.7.44, for Linux (x86_64)
--
-- Host: localhost    Database: server1
-- ------------------------------------------------------
-- Server version	5.7.44

/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET @OLD_CHARACTER_SET_RESULTS=@@CHARACTER_SET_RESULTS */;
/*!40101 SET @OLD_COLLATION_CONNECTION=@@COLLATION_CONNECTION */;
/*!40101 SET NAMES utf8 */;
/*!40103 SET @OLD_TIME_ZONE=@@TIME_ZONE */;
/*!40103 SET TIME_ZONE='+00:00' */;
/*!40014 SET @OLD_UNIQUE_CHECKS=@@UNIQUE_CHECKS, UNIQUE_CHECKS=0 */;
/*!40014 SET @OLD_FOREIGN_KEY_CHECKS=@@FOREIGN_KEY_CHECKS, FOREIGN_KEY_CHECKS=0 */;
/*!40101 SET @OLD_SQL_MODE=@@SQL_MODE, SQL_MODE='NO_AUTO_VALUE_ON_ZERO' */;
/*!40111 SET @OLD_SQL_NOTES=@@SQL_NOTES, SQL_NOTES=0 */;

--
-- Table structure for table `Role`
--

DROP TABLE IF EXISTS `Role`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `Role` (
  `ID` int(11) NOT NULL AUTO_INCREMENT,
  `RoleName` tinyblob NOT NULL,
  `Account` varchar(32) COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `RoleData` longblob,
  `LastModify` datetime DEFAULT NULL,
  PRIMARY KEY (`RoleName`(32)),
  UNIQUE KEY `ID` (`ID`),
  KEY `Account` (`Account`)
) ENGINE=InnoDB AUTO_INCREMENT=16 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;
/*!40103 SET TIME_ZONE=@OLD_TIME_ZONE */;

/*!40101 SET SQL_MODE=@OLD_SQL_MODE */;
/*!40014 SET FOREIGN_KEY_CHECKS=@OLD_FOREIGN_KEY_CHECKS */;
/*!40014 SET UNIQUE_CHECKS=@OLD_UNIQUE_CHECKS */;
/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
/*!40111 SET SQL_NOTES=@OLD_SQL_NOTES */;

-- Dump completed on 2026-07-17  1:58:46
