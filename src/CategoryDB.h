#pragma once
#include "Icons.h"
#include <string>
#include <vector>

struct MenuItem {
  std::wstring text;
  std::string id; // slug
  std::vector<MenuItem> children;
  bool isTitle = false;
  HeaderIcon iconType = HeaderIcon::None;
};

// Hierarchical Categories for Divar
static const std::vector<MenuItem> g_MenuCategories = {
    {L"همهٔ دسته‌ها", "", {}},

    {L"املاک",
     "real-estate",
     {{L"فروش مسکونی",
       "buy-residential",
       {{L"آپارتمان", "buy-apartment"},
        {L"خانه و ویلا", "buy-villa"},
        {L"زمین و کلنگی", "buy-old-house"}}},
      {L"اجاره مسکونی",
       "rent-residential",
       {{L"آپارتمان", "rent-apartment"}, {L"خانه و ویلا", "rent-villa"}}},
      {L"فروش اداری و تجاری", "buy-commercial", {}},
      {L"اجاره اداری و تجاری", "rent-commercial", {}},
      {L"اجاره کوتاه مدت", "temporary-rent", {}},
      {L"پروژه‌های ساخت و ساز",
       "construction-projects",
       {}}}},

    {L"وسایل نقلیه",
     "vehicles",
     {{L"خودرو",
       "cars",
       {{L"سواری", "cars"},
        {L"کلاسیک", "classic-cars"},
        {L"اجاره‌ای", "rental-cars"}}},
      {L"موتورسیکلت", "motorcycles", {}},
      {L"قطعات یدکی و لوازم", "parts-accessories", {}},
      {L"قایق و لوازم جانبی", "boat", {}}}},

    {L"کالای دیجیتال",
     "electronic-devices",
     {{L"موبایل و تبلت",
       "mobile-tablet",
       {{L"گوشی موبایل", "mobile-phones"},
        {L"تبلت", "tablet"},
        {L"لوازم جانبی", "mobile-tablet-accessories"}}},
      {L"رایانه و لپ‌تاپ",
       "computers",
       {{L"لپ‌تاپ", "laptops-accessories"}}},
      {L"کنسول بازی", "game-consoles-and-video-games", {}},
      {L"صوتی و تصویری", "audio-video", {}}}},

    {L"خانه و آشپزخانه",
     "home-kitchen",
     {{L"لوازم خانگی برقی", "appliance", {}},
      {L"ظروف و لوازم آشپزخانه", "kitchenware", {}},
      {L"خوردنی و آشامیدنی", "food-and-drink", {}},
      {L"ابزار ساختمانی", "building-equipment", {}},
      {L"فرش و گلیم", "carpets", {}},
      {L"مبلمان و دکوراسیون", "furniture-and-home-decore", {}}}},

    {L"خدمات",
     "services",
     {{L"پذیرایی و مراسم", "catering-services", {}},
      {L"نظافت و بهداشت", "cleaning-services", {}},
      {L"حمل و نقل", "transportation-services", {}},
      {L"رایانه و موبایل", "computer-mobile-services", {}},
      {L"مالی و حسابداری", "finance-accounting-services", {}},
      {L"آموزشی", "teaching", {}},
      {L"آرایشگری و زیبایی", "barbershop-and-beautysalon", {}}}},

    {L"وسایل شخصی",
     "personal-goods",
     {{L"کیف، کفش و لباس", "apparel", {}},
      {L"تزیینی", "jewelry", {}},
      {L"آرایشی و بهداشتی", "beauty-and-personal-care", {}},
      {L"کفش و لباس بچه", "child-apparel", {}}}},

    {L"سرگرمی و فراغت",
     "entertainment",
     {{L"بلیط", "ticket", {}},
      {L"تور و چارتر", "tours", {}},
      {L"کتاب و مجله", "book-and-magazine", {}},
      {L"دوچرخه و اسکیت", "bicycle", {}},
      {L"حیوانات", "animals", {}},
      {L"کلکسیون", "collection", {}}}},

    {L"اجتماعی",
     "social",
     {{L"رویداد", "event", {}},
      {L"داوطلبانه", "volunteers", {}},
      {L"گم‌شده", "lost-and-found", {}}}},

    {L"تجهیزات و صنعتی",
     "industrial-equipment",
     {{L"مواد اولیه", "raw-materials", {}},
      {L"ماشین‌آلات", "machinery", {}},
      {L"تجهیزات فروشگاهی", "shop-equipment", {}},
      {L"ابزارآلات", "industrial-tools", {}}}},

    {L"استخدام و کاریابی",
     "jobs",
     {{L"اداری و مدیریت", "administrative-jobs", {}},
      {L"سرایداری و نظافت", "janitorial-jobs", {}},
      {L"معماری و عمران", "civil-engineering-jobs", {}},
      {L"رایانه و فناوری", "it-computer-jobs", {}},
      {L"مالی و حسابداری", "finance-accounting-jobs", {}},
      {L"فنی و پیشه‌وری", "technical-jobs", {}}}}};