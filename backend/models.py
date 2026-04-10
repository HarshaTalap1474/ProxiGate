from sqlalchemy import Column, Integer, String, DateTime
from datetime import datetime
from database import Base

class User(Base):
    __tablename__ = "users"

    id = Column(Integer, primary_key=True, index=True)
    hardwareId = Column(Integer, unique=True, index=True)
    role = Column(String) # "ADMIN" or "USER"
    name = Column(String)
    dateAdded = Column(DateTime, default=datetime.utcnow)

class SystemConfig(Base):
    __tablename__ = "system_config"

    id = Column(Integer, primary_key=True, index=True, default=1)
    adminPin = Column(String, default="")
    rawPin = Column(String, default="")
    pendingCommand = Column(String, default="NONE")
    cameraIp = Column(String, default="")
