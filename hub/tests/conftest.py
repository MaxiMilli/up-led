import pytest
import subprocess
import os
import shutil
from httpx import AsyncClient
from fastapi.testclient import TestClient
from sqlalchemy.ext.asyncio import create_async_engine, AsyncSession
from sqlalchemy.orm import sessionmaker

from src.hub_api.routes import app
from src.database.models import Base
from src.hub_api.dependencies import get_db
from src.database.models import init_db

# Use an in-memory SQLite database for testing
TEST_DATABASE_URL = "sqlite+aiosqlite:///:memory:"

# Fixture to run setup before all tests
@pytest.fixture(scope="session", autouse=True)
async def setup_database():
    # Run the database initialization command
    subprocess.run(["python", "-m", "src.database.models"], check=True)

    await init_db()
    
    # Ensure the database is cleaned up after tests
    yield
    if os.path.exists("led_control_test.db"):
        os.remove("led_control_test.db")

# Fixture for temporary directory setup
@pytest.fixture
def setup_test_directory():
    # Set up a test directory and clean up before and after tests
    test_dir = os.path.join(os.getcwd(), "test_songs")
    if os.path.exists(test_dir):
        shutil.rmtree(test_dir)
    os.makedirs(test_dir, exist_ok=True)
    yield test_dir
    shutil.rmtree(test_dir)

@pytest.fixture
async def async_engine():
    engine = create_async_engine(TEST_DATABASE_URL)
    async with engine.begin() as conn:
        await conn.run_sync(Base.metadata.create_all)
    yield engine
    async with engine.begin() as conn:
        await conn.run_sync(Base.metadata.drop_all)
    await engine.dispose()

@pytest.fixture
async def async_session(async_engine):
    async_session = sessionmaker(
        async_engine, class_=AsyncSession, expire_on_commit=False
    )
    async with async_session() as session:
        yield session

@pytest.fixture
async def client(async_session):
    async def override_get_db():
        yield async_session

    app.dependency_overrides[get_db] = override_get_db
    
    async with AsyncClient(app=app, base_url="http://test") as ac:
        yield ac
    
    app.dependency_overrides.clear()
