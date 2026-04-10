from pydantic import BaseModel
from typing import Optional
from datetime import datetime

class UserCreate(BaseModel):
    name: str
    role: str

class UserResponse(BaseModel):
    id: int
    hardwareId: int
    role: str
    name: str
    dateAdded: datetime

    class Config:
        orm_mode = True

class PinUpdate(BaseModel):
    newPin: str
    pinType: str = "ADMIN"

class DeviceResult(BaseModel):
    action: str
    hardwareId: int
    success: bool
    
class CamIpUpdate(BaseModel):
    ip: str
