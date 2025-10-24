from django.urls import path
from . import views

urlpatterns = [
    path('beacon/', views.beacon, name='beacon'),
    path('upload/', views.upload, name='upload'),
]
