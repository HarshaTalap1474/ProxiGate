from fastapi import FastAPI, Depends, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from sqlalchemy.orm import Session
from typing import List
from datetime import datetime, timedelta

lock_last_seen = None
cam_last_seen = None

import models
import schemas
from database import engine, get_db

models.Base.metadata.create_all(bind=engine)

app = FastAPI(title="ProxiGate API")

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)


def get_or_create_config(db: Session):
    config = db.query(models.SystemConfig).filter(models.SystemConfig.id == 1).first()
    if not config:
        config = models.SystemConfig(id=1, adminPin="", rawPin="", pendingCommand="NONE", cameraIp="")
        db.add(config)
        db.commit()
        db.refresh(config)
    return config

@app.get("/api/users", response_model=List[schemas.UserResponse])
def get_users(db: Session = Depends(get_db)):
    users = db.query(models.User).all()
    return users

@app.post("/api/users/enroll")
def enroll_user(user: schemas.UserCreate, db: Session = Depends(get_db)):
    config = get_or_create_config(db)
    if config.pendingCommand != "NONE":
        raise HTTPException(status_code=400, detail="Another hardware command is already pending.")
    
    # Find next available hardwareId based on role
    existing_users = db.query(models.User).filter(models.User.role == user.role).all()
    existing_ids = [u.hardwareId for u in existing_users]
    
    if user.role == "ADMIN":
        available_ids = [i for i in range(1, 11) if i not in existing_ids]
    else:
        available_ids = [i for i in range(11, 257) if i not in existing_ids]
        
    if not available_ids:
        raise HTTPException(status_code=400, detail=f"No available slots for {user.role}")
        
    next_id = available_ids[0]
    
    # We don't save the user to the DB yet, we just trigger the command
    config.pendingCommand = f"ENROLL_{next_id}_{user.role}_{user.name}"
    db.commit()
    
    return {"message": "Enrollment command sent to hardware", "hardwareId": next_id}

@app.delete("/api/users/{user_id}")
def delete_user(user_id: int, db: Session = Depends(get_db)):
    user = db.query(models.User).filter(models.User.id == user_id).first()
    if not user:
        raise HTTPException(status_code=404, detail="User not found")
        
    config = get_or_create_config(db)
    if config.pendingCommand != "NONE":
        raise HTTPException(status_code=400, detail="Another hardware command is already pending.")
        
    config.pendingCommand = f"DELETE_{user.hardwareId}"
    db.delete(user)
    db.commit()
    return {"message": "User deleted and command sent to hardware"}

@app.post("/api/settings/pin")
def update_pin(pin_data: schemas.PinUpdate, db: Session = Depends(get_db)):
    config = get_or_create_config(db)
    if config.pendingCommand != "NONE":
        raise HTTPException(status_code=400, detail="Another hardware command is already pending.")
        
    config.adminPin = pin_data.newPin
    config.rawPin = pin_data.newPin
    
    if pin_data.pinType == "ADMIN":
        config.pendingCommand = "UPDATE_ADMIN_PIN"
    else:
        config.pendingCommand = "UPDATE_USER_PIN"
        
    db.commit()
    return {"message": "Pin update command sent to hardware"}

@app.get("/api/system/status")
def get_system_status(db: Session = Depends(get_db)):
    config = get_or_create_config(db)
    
    now = datetime.now()
    lock_conn = lock_last_seen is not None and (now - lock_last_seen) < timedelta(seconds=10)
    cam_conn = cam_last_seen is not None and (now - cam_last_seen) < timedelta(seconds=20)
    
    return {
        "pendingCommand": config.pendingCommand,
        "cameraIp": config.cameraIp,
        "lockConnected": lock_conn,
        "camConnected": cam_conn
    }

@app.post("/api/system/clear")
def clear_system_command(db: Session = Depends(get_db)):
    config = get_or_create_config(db)
    config.pendingCommand = "NONE"
    db.commit()
    return {"message": "Pending command cleared"}

# --- Device API ---

@app.get("/api/device/poll")
def device_poll(db: Session = Depends(get_db)):
    global lock_last_seen
    lock_last_seen = datetime.now()
    
    config = get_or_create_config(db)
    cmd = config.pendingCommand
    
    if cmd == "NONE":
        return {"command": "NONE"}
        
    if cmd.startswith("ENROLL_"):
        parts = cmd.split("_")
        hw_id = int(parts[1])
        return {"command": "ENROLL", "hardwareId": hw_id}
        
    if cmd.startswith("DELETE_"):
        parts = cmd.split("_")
        hw_id = int(parts[1])
        # Clear the command immediately since the ESP32 doesn't need to post a result for DELETE
        config.pendingCommand = "NONE"
        db.commit()
        return {"command": "DELETE", "hardwareId": hw_id}
        
    if cmd in ("UPDATE_ADMIN_PIN", "UPDATE_USER_PIN"):
        raw_pin = config.rawPin
        config.rawPin = "" # Clear temporary raw PIN
        config.pendingCommand = "NONE" # Assume lock handles it
        db.commit()
        return {"command": cmd, "pin": raw_pin}
        
    return {"command": "NONE"}

@app.post("/api/device/result")
def device_result(result: schemas.DeviceResult, db: Session = Depends(get_db)):
    config = get_or_create_config(db)
    
    if result.action == "ENROLL_RESULT":
        if result.success and config.pendingCommand.startswith(f"ENROLL_{result.hardwareId}"):
            parts = config.pendingCommand.split("_", 3)
            hw_id = int(parts[1])
            role = parts[2]
            name = parts[3]
            
            new_user = models.User(hardwareId=hw_id, role=role, name=name)
            db.add(new_user)
            config.pendingCommand = "NONE"
            db.commit()
            return {"status": "User saved"}
        else:
            config.pendingCommand = "NONE"
            db.commit()
            return {"status": "Enrollment failed or mismatched"}

@app.get("/api/device/cam_ip")
def register_cam_ip(ip: str, db: Session = Depends(get_db)):
    global cam_last_seen
    cam_last_seen = datetime.now()
    
    config = get_or_create_config(db)
    config.cameraIp = ip
    db.commit()
    return {"status": "registered"}
